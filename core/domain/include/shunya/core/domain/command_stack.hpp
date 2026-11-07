#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <optional>
#include <functional>

namespace shunya::core::domain {

class ICommand {
public:
    virtual ~ICommand() = default;

    [[nodiscard]] virtual base::Result<void> execute() = 0;
    [[nodiscard]] virtual base::Result<void> undo() = 0;
    [[nodiscard]] virtual base::Result<void> redo() { return execute(); }

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] virtual bool can_merge_with([[maybe_unused]] const ICommand& other) const noexcept {
        return false;
    }
    [[nodiscard]] virtual bool merge_with([[maybe_unused]] std::unique_ptr<ICommand> other) noexcept {
        return false;
    }
};

class CompositeCommand : public ICommand {
public:
    explicit CompositeCommand(std::string name) : name_(std::move(name)) {}

    void add_command(std::unique_ptr<ICommand> cmd) {
        if (cmd) {
            commands_.push_back(std::move(cmd));
        }
    }

    [[nodiscard]] std::size_t command_count() const noexcept { return commands_.size(); }
    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }

    [[nodiscard]] base::Result<void> execute() override;
    [[nodiscard]] base::Result<void> undo() override;
    [[nodiscard]] base::Result<void> redo() override;

    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    std::string name_;
    std::vector<std::unique_ptr<ICommand>> commands_;
};

class CommandStack {
public:
    explicit CommandStack(std::size_t max_history = 100)
        : max_history_(max_history) {}

    [[nodiscard]] base::Result<void> push_and_execute(std::unique_ptr<ICommand> cmd);

    [[nodiscard]] base::Result<void> undo();
    [[nodiscard]] base::Result<void> redo();

    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;

    [[nodiscard]] std::string_view undo_name() const noexcept;
    [[nodiscard]] std::string_view redo_name() const noexcept;

    // Transaction grouping
    [[nodiscard]] base::Result<void> begin_transaction(std::string name);
    [[nodiscard]] base::Result<void> commit_transaction();
    [[nodiscard]] base::Result<void> rollback_transaction();
    [[nodiscard]] bool is_in_transaction() const noexcept { return active_transaction_.has_value(); }

    // Clean (save state) tracking
    [[nodiscard]] bool is_clean() const noexcept { return current_index_ == clean_index_; }
    void mark_clean() noexcept { clean_index_ = current_index_; }

    [[nodiscard]] std::size_t count() const noexcept { return commands_.size(); }
    [[nodiscard]] std::size_t current_index() const noexcept { return current_index_; }
    [[nodiscard]] std::size_t max_history() const noexcept { return max_history_; }
    void set_max_history(std::size_t limit) noexcept;

    struct CommandHistoryEntry {
        size_t index{0};
        std::string name;
        bool is_undone{false};
        bool is_current{false};
    };

    [[nodiscard]] std::vector<CommandHistoryEntry> history_entries() const;
    [[nodiscard]] base::Result<void> rollback_to_index(size_t target_index);

    void clear() noexcept;

private:
    std::size_t max_history_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    std::size_t current_index_ = 0; // index pointing to next redo position (number of executed commands)
    std::optional<std::size_t> clean_index_ = 0;

    std::optional<CompositeCommand> active_transaction_;
};

// Generic lambda-based command helper
class LambdaCommand : public ICommand {
public:
    using ActionFunc = std::function<base::Result<void>()>;

    LambdaCommand(std::string name, ActionFunc do_func, ActionFunc undo_func)
        : name_(std::move(name)), do_func_(std::move(do_func)), undo_func_(std::move(undo_func)) {}

    [[nodiscard]] base::Result<void> execute() override { return do_func_(); }
    [[nodiscard]] base::Result<void> undo() override { return undo_func_(); }
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

private:
    std::string name_;
    ActionFunc do_func_;
    ActionFunc undo_func_;
};

} // namespace shunya::core::domain
