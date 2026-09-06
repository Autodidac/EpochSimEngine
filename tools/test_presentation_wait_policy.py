"""Source-wiring regressions, complementary to native policy and Vulkan captures.

These checks do not execute the renderer or establish runtime/visual acceptance.
They protect the actual draw_frame initializer and Vulkan argument positions,
with rejected mutations reproducing report-gated and divergent capture waits.
"""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8")
TOKEN = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'
    r"[0-9][0-9A-Za-z_']*|'(?:\\.|[^'\\])*'|"
    r"[A-Za-z_]\w*|::|&&|\|\||==|!=|<=|>=|->|[^\s]", re.S)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def tokens(source):
    return [match[0] for match in TOKEN.finditer(source)
            if not match[0].startswith(("//", "/*"))]


def closing(items, start, left, right):
    depth = 0
    for index in range(start, len(items)):
        depth += (items[index] == left) - (items[index] == right)
        if depth == 0:
            return index
    raise AssertionError(f"Unbalanced {left}{right}")


def frame_body(source):
    items = tokens(source)
    starts = [index for index in range(len(items) - 2)
              if items[index:index + 3] == ["bool", "draw_frame", "("]]
    require(len(starts) == 1, "Expected exactly one production draw_frame")
    parameters_end = closing(items, starts[0] + 2, "(", ")")
    require(items[parameters_end + 1] == "{", "Missing draw_frame definition")
    begin = parameters_end + 1
    return items[begin + 1:closing(items, begin, "{", "}")]


def calls(items, name):
    result = []
    for index in range(len(items) - 1):
        if items[index:index + 2] != [name, "("]:
            continue
        end = closing(items, index + 1, "(", ")")
        arguments, argument, depth = [], [], 0
        for token in items[index + 2:end]:
            if token == "," and depth == 0:
                arguments.append(argument)
                argument = []
                continue
            depth += (token in ("(", "[", "{")) - (token in (")", "]", "}"))
            argument.append(token)
        result.append(arguments + [argument])
    return result


def check_wiring(source):
    body = frame_body(source)
    declarations = [index for index in range(len(body) - 1)
                    if body[index:index + 2] == ["gpu_timeout_ns", "="]]
    require(len(declarations) == 1, "Expected one immutable frame wait budget")
    index = declarations[0]
    require(body[index - 4:index] == ["const", "std", "::", "uint64_t"],
            "Wait budget must remain const uint64_t")
    expression = body[index + 2:body.index(";", index)]
    expected = ["presentation_wait_timeout_ns", "(", "cpu_physical_device", ")"]
    require(expression in (expected, ["sandhybrid", "::"] + expected),
            "Frame wait must depend only on physical device, never report mode")
    waits, acquires = calls(body, "vkWaitForFences"), calls(body, "vkAcquireNextImageKHR")
    require(len(waits) == 3 and len(acquires) == 1,
            "Audit all frame, image, capture and acquire waits")
    require(all(len(args) == 5 and args[4] == ["gpu_timeout_ns"] for args in waits),
            "Frame/image/capture fence timeout diverged from shared policy")
    require(len(acquires[0]) == 6 and acquires[0][2] == ["gpu_timeout_ns"],
            "Swapchain acquire timeout diverged from shared policy")


def check_diagnostics(source):
    body = frame_body(source)
    declarations = [index for index in range(len(body) - 1)
                    if body[index:index + 2] == ["gpu_timeout_seconds", "="]]
    require(len(declarations) == 1, "Diagnostic must report the actual wait seconds")
    index = declarations[0]
    require(body[index - 2:index] == ["const", "auto"],
            "Diagnostic seconds must remain immutable")
    require(body[index + 2:body.index(";", index)] ==
            ["std", "::", "to_string", "(", "gpu_timeout_ns", "/",
             "1'000'000'000ull", ")"], "Diagnostic must report the actual wait seconds")
    timeouts = [args[0] for args in calls(body, "runtime_error")
                if args and any(token.startswith('"') and "timed out" in token.lower()
                                for token in args[0])]
    require(len(timeouts) == 4, "Every presentation timeout needs its own diagnostic")
    for diagnostic in timeouts:
        literals = " ".join(token.lower() for token in diagnostic if token.startswith('"'))
        require("5 seconds" not in literals and "first simulation submission stalled" not in literals,
                "Diagnostic must not misreport duration or assume first simulation work")
        require("gpu_timeout_seconds" in diagnostic,
                "Diagnostic must report the actual wait seconds")


def replace_policy(source, expression):
    result, count = re.subn(
        r"(const\s+std::uint64_t\s+gpu_timeout_ns\s*=\s*)[^;]+;",
        lambda match: match[1] + expression + ";", source)
    require(count == 1, "Mutation must replace exactly one policy initializer")
    return result


def replace_wait(source, result_name, function):
    pattern = (r"(\b" + re.escape(result_name) + r"\s*=\s*" + re.escape(function)
               + r"\([^;]*?)\bgpu_timeout_ns\b")
    result, count = re.subn(pattern, lambda match: match[1] + "5'000'000'000ull", source)
    require(count == 1, "Mutation must replace exactly one real wait argument")
    return result


class PresentationWaitPolicyContract(unittest.TestCase):
    def test_production_wait_arguments_share_device_only_policy(self):
        check_wiring(SOURCE)

    def test_production_timeout_diagnostics_report_actual_budget(self):
        check_diagnostics(SOURCE)

    def test_every_report_dependent_policy_mutation_is_rejected(self):
        for report in ("interactive_acceptance_report", "runtime_acceptance_report",
                       "long_cycle_acceptance_report", "simulation_profile_report"):
            with self.subTest(report=report):
                mutant = replace_policy(
                    SOURCE, "presentation_wait_timeout_ns(cpu_physical_device && "
                    f"!config.{report}.empty())")
                with self.assertRaisesRegex(AssertionError, "never report mode"):
                    check_wiring(mutant)

    def test_each_divergent_wait_argument_is_rejected(self):
        for name, function in (("fence_result", "vkWaitForFences"),
                               ("image_fence_result", "vkWaitForFences"),
                               ("capture_result", "vkWaitForFences"),
                               ("acquire_result", "vkAcquireNextImageKHR")):
            with self.subTest(wait=name):
                with self.assertRaisesRegex(AssertionError, "diverged"):
                    check_wiring(replace_wait(SOURCE, name, function))

    def test_commented_correct_policy_cannot_hide_bad_initializer(self):
        mutant = replace_policy(SOURCE, "5'000'000'000ull")
        mutant += "\n// presentation_wait_timeout_ns(cpu_physical_device)\n"
        with self.assertRaisesRegex(AssertionError, "never report mode"):
            check_wiring(mutant)

    def test_hardcoded_diagnostic_mutation_is_rejected(self):
        mutant, count = re.subn(
            r"std::to_string\s*\(\s*gpu_timeout_ns\s*/\s*1'000'000'000ull\s*\)",
            '"5"', SOURCE, count=1)
        self.assertEqual(count, 1, "Mutation must replace a real diagnostic")
        with self.assertRaisesRegex(AssertionError, "actual wait seconds"):
            check_diagnostics(mutant)


if __name__ == "__main__":
    unittest.main()
