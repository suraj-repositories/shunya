#include "shunya/core/domain/command_stack.hpp"

namespace shunya::core::domain {

base::Result<void> CompositeCommand::execute() {
    std::size_t executed = 0;
    for (std::size_t i = 0; i < commands_.size(); ++i) {
        auto res = commands_[i]->execute();
        if (res.is_err()) {
            // Roll back previously executed sub-commands in reverse order
            for (std::size_t j = executed; j > 0; --j) {
                [[maybe_unused]] auto rollback_res = commands_[j - 1]->undo();
            }
            return res;
        }
        ++executed;
    }
    return base::Result<void>::ok();
}

base::Result<void> CompositeCommand::undo() {
    for (std::size_t i = commands_.size(); i > 0; --i) {
        auto res = commands_[i - 1]->undo();
        if (res.is_err()) {
            return res;
        }
    }
    return base::Result<void>::ok();
}

base::Result<void> CompositeCommand::redo() {
    for (const auto& cmd : commands_) {
        auto res = cmd->redo();
        if (res.is_err()) {
            return res;
        }
    }
    return base::Result<void>::ok();
}

base::Result<void> CommandStack::push_and_execute(std::unique_ptr<ICommand> cmd) {
    if (!cmd) {
        return base::Result<void>::err({
            .code = 2205,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Cannot push null command to command stack",
            .details = ""
        });
    }

    if (active_transaction_.has_value()) {
        auto exec_res = cmd->execute();
        if (exec_res.is_err()) {
            return exec_res;
        }
        active_transaction_->add_command(std::move(cmd));
        return base::Result<void>::ok();
    }

    // Try merge with previous command if supported
    if (current_index_ > 0 && commands_[current_index_ - 1]->can_merge_with(*cmd)) {
        if (commands_[current_index_ - 1]->merge_with(std::move(cmd))) {
            return base::Result<void>::ok();
        }
    }

    auto exec_res = cmd->execute();
    if (exec_res.is_err()) {
        return exec_res;
    }

    // Discard any commands after current_index_ (branching history)
    if (current_index_ < commands_.size()) {
        if (clean_index_.has_value() && clean_index_.value() > current_index_) {
            clean_index_ = std::nullopt; // Saved state is now orphaned
        }
        commands_.erase(commands_.begin() + current_index_, commands_.end());
    }

    commands_.push_back(std::move(cmd));
    ++current_index_;

    // Bound history to max_history_
    if (commands_.size() > max_history_) {
        commands_.erase(commands_.begin());
        --current_index_;
        if (clean_index_.has_value()) {
            if (clean_index_.value() > 0) {
                clean_index_ = clean_index_.value() - 1;
            } else {
                clean_index_ = std::nullopt; // Clean state evicted from history
            }
        }
    }

    return base::Result<void>::ok();
}

base::Result<void> CommandStack::undo() {
    if (!can_undo()) {
        return base::Result<void>::err({
            .code = 2201,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Warning,
            .message = "No commands available to undo",
            .details = ""
        });
    }

    --current_index_;
    auto res = commands_[current_index_]->undo();
    if (res.is_err()) {
        // Restore index on undo failure
        ++current_index_;
        return res;
    }

    return base::Result<void>::ok();
}

base::Result<void> CommandStack::redo() {
    if (!can_redo()) {
        return base::Result<void>::err({
            .code = 2202,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Warning,
            .message = "No commands available to redo",
            .details = ""
        });
    }

    auto res = commands_[current_index_]->redo();
    if (res.is_err()) {
        return res;
    }

    ++current_index_;
    return base::Result<void>::ok();
}

bool CommandStack::can_undo() const noexcept {
    return current_index_ > 0 && !active_transaction_.has_value();
}

bool CommandStack::can_redo() const noexcept {
    return current_index_ < commands_.size() && !active_transaction_.has_value();
}

std::string_view CommandStack::undo_name() const noexcept {
    if (can_undo()) {
        return commands_[current_index_ - 1]->name();
    }
    return "";
}

std::string_view CommandStack::redo_name() const noexcept {
    if (can_redo()) {
        return commands_[current_index_]->name();
    }
    return "";
}

base::Result<void> CommandStack::begin_transaction(std::string name) {
    if (active_transaction_.has_value()) {
        return base::Result<void>::err({
            .code = 2203,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Cannot begin transaction: a transaction is already active",
            .details = ""
        });
    }

    active_transaction_ = CompositeCommand(std::move(name));
    return base::Result<void>::ok();
}

base::Result<void> CommandStack::commit_transaction() {
    if (!active_transaction_.has_value()) {
        return base::Result<void>::err({
            .code = 2204,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Cannot commit: no active transaction",
            .details = ""
        });
    }

    auto composite = std::make_unique<CompositeCommand>(std::move(active_transaction_.value()));
    active_transaction_ = std::nullopt;

    if (composite->empty()) {
        return base::Result<void>::ok();
    }

    if (current_index_ < commands_.size()) {
        if (clean_index_.has_value() && clean_index_.value() > current_index_) {
            clean_index_ = std::nullopt;
        }
        commands_.erase(commands_.begin() + current_index_, commands_.end());
    }

    commands_.push_back(std::move(composite));
    ++current_index_;

    if (commands_.size() > max_history_) {
        commands_.erase(commands_.begin());
        --current_index_;
        if (clean_index_.has_value()) {
            if (clean_index_.value() > 0) {
                clean_index_ = clean_index_.value() - 1;
            } else {
                clean_index_ = std::nullopt;
            }
        }
    }

    return base::Result<void>::ok();
}

base::Result<void> CommandStack::rollback_transaction() {
    if (!active_transaction_.has_value()) {
        return base::Result<void>::err({
            .code = 2204,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Cannot rollback: no active transaction",
            .details = ""
        });
    }

    auto res = active_transaction_->undo();
    active_transaction_ = std::nullopt;
    return res;
}

void CommandStack::set_max_history(std::size_t limit) noexcept {
    max_history_ = limit > 0 ? limit : 1;
    while (commands_.size() > max_history_) {
        commands_.erase(commands_.begin());
        if (current_index_ > 0) --current_index_;
        if (clean_index_.has_value()) {
            if (clean_index_.value() > 0) {
                clean_index_ = clean_index_.value() - 1;
            } else {
                clean_index_ = std::nullopt;
            }
        }
    }
}

std::vector<CommandStack::CommandHistoryEntry> CommandStack::history_entries() const {
    std::vector<CommandHistoryEntry> list;
    list.reserve(commands_.size());
    for (size_t i = 0; i < commands_.size(); ++i) {
        list.push_back(CommandHistoryEntry{
            .index = i,
            .name = std::string(commands_[i]->name()),
            .is_undone = (i >= current_index_),
            .is_current = (i + 1 == current_index_)
        });
    }
    return list;
}

base::Result<void> CommandStack::rollback_to_index(size_t target_index) {
    if (active_transaction_.has_value()) {
        return base::Result<void>::err({
            .code = 2206,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Cannot rollback history during active transaction",
            .details = ""
        });
    }

    if (target_index > commands_.size()) {
        target_index = commands_.size();
    }

    while (current_index_ > target_index) {
        auto res = undo();
        if (res.is_err()) return res;
    }

    while (current_index_ < target_index) {
        auto res = redo();
        if (res.is_err()) return res;
    }

    return base::Result<void>::ok();
}

void CommandStack::clear() noexcept {
    commands_.clear();
    current_index_ = 0;
    clean_index_ = 0;
    active_transaction_ = std::nullopt;
}

} // namespace shunya::core::domain
