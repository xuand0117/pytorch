#include <c10/util/irange.h>
#include <torch/csrc/autograd/cpp_hook.h>
#include <torch/csrc/autograd/custom_function.h>
#include <torch/csrc/autograd/variable.h>

#include <utility>

namespace {
using torch::autograd::Variable;
void check_single_result(
    const at::TensorBase& value,
    const at::TensorBase& result,
    std::string hook_name) {
  if (!value.defined()) {
    throw std::runtime_error(
        "can't replace a empty gradient with a non-empty value");
  }
  torch::autograd::check_variable_result(value, result, std::move(hook_name));
}
} // namespace

namespace torch {
namespace autograd {

// NOLINTNEXTLINE(modernize-pass-by-value)
CppFunctionTensorPreHook::CppFunctionTensorPreHook(
    const std::shared_ptr<hooks_list>& hooks,
    int value_idx)
    : hooks_(hooks), value_idx_(value_idx) {}

variable_list CppFunctionTensorPreHook::operator()(
    const variable_list& values) {
  auto value = values[value_idx_];
  for (const auto i : c10::irange(hooks_->size())) {
    auto& hook = (*hooks_)[i];
    if (!hook) {
      // hook was removed
      continue;
    }
    auto res = hook(value);
    if (!res.defined()) {
      // Don't change gradient
      continue;
    }
    check_single_result(value, res, c10::to_string(i));
    value = std::move(res);
  }
  variable_list results(values);
  results[value_idx_] = value;
  return results;
}

CppFunctionSingleTensorPreHook::CppFunctionSingleTensorPreHook(
    std::function<at::TensorBase(const at::TensorBase&)> hook,
    int value_idx)
    : hook_(std::move(hook)), value_idx_(value_idx) {}

variable_list CppFunctionSingleTensorPreHook::operator()(
    const variable_list& values) {
  auto value = values[value_idx_];
  auto res = hook_(value);
  TORCH_INTERNAL_ASSERT(
      !res.defined(),
      "CppFunctionSingleTensorPreHook currently only supports hooks that don't return");
  variable_list results(values);
  return results;
}

} // namespace autograd
} // namespace torch
