#pragma once

#include "shunya/core/base/error.hpp"

#include <variant>
#include <optional>
#include <utility>
#include <stdexcept>

namespace shunya::core::base {

template <typename T>
class Result {
public:
    Result(const T& val) : data_(val) {}
    Result(T&& val) : data_(std::move(val)) {}
    Result(Error err) : data_(std::move(err)) {}

    [[nodiscard]] bool is_ok() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] bool is_err() const noexcept {
        return std::holds_alternative<Error>(data_);
    }

    explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] const T& value() const & {
        if (is_err()) {
            throw std::runtime_error("Attempted to access value on error Result: " + error().format());
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T& value() & {
        if (is_err()) {
            throw std::runtime_error("Attempted to access value on error Result: " + error().format());
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        if (is_err()) {
            throw std::runtime_error("Attempted to access value on error Result: " + error().format());
        }
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] const Error& error() const noexcept {
        return std::get<Error>(data_);
    }

    [[nodiscard]] T value_or(T default_value) const {
        if (is_ok()) {
            return std::get<T>(data_);
        }
        return default_value;
    }

    static Result<T> ok(T val) {
        return Result<T>(std::move(val));
    }

    static Result<T> err(Error error) {
        return Result<T>(std::move(error));
    }

private:
    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    Result() : err_(std::nullopt) {}
    Result(Error err) : err_(std::move(err)) {}

    [[nodiscard]] bool is_ok() const noexcept {
        return !err_.has_value();
    }

    [[nodiscard]] bool is_err() const noexcept {
        return err_.has_value();
    }

    explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] const Error& error() const noexcept {
        return err_.value();
    }

    static Result<void> ok() {
        return Result<void>();
    }

    static Result<void> err(Error error) {
        return Result<void>(std::move(error));
    }

private:
    std::optional<Error> err_;
};

} // namespace shunya::core::base
