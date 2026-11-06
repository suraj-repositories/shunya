#include "tests/test_framework.hpp"
#include "shunya/core/domain/command_stack.hpp"

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("CommandStack basic execute, undo, and redo") {
    CommandStack stack;
    int state = 0;

    auto cmd1 = std::make_unique<LambdaCommand>(
        "Add 10",
        [&]() -> base::Result<void> { state += 10; return base::Result<void>::ok(); },
        [&]() -> base::Result<void> { state -= 10; return base::Result<void>::ok(); }
    );

    auto cmd2 = std::make_unique<LambdaCommand>(
        "Multiply by 2",
        [&]() -> base::Result<void> { state *= 2; return base::Result<void>::ok(); },
        [&]() -> base::Result<void> { state /= 2; return base::Result<void>::ok(); }
    );

    CHECK(!stack.can_undo());
    CHECK(!stack.can_redo());

    REQUIRE(stack.push_and_execute(std::move(cmd1)).is_ok());
    CHECK(state == 10);
    CHECK(stack.can_undo());
    CHECK(!stack.can_redo());
    CHECK(stack.undo_name() == "Add 10");

    REQUIRE(stack.push_and_execute(std::move(cmd2)).is_ok());
    CHECK(state == 20);
    CHECK(stack.undo_name() == "Multiply by 2");

    // Undo Multiply by 2
    REQUIRE(stack.undo().is_ok());
    CHECK(state == 10);
    CHECK(stack.can_undo());
    CHECK(stack.can_redo());
    CHECK(stack.redo_name() == "Multiply by 2");
    CHECK(stack.undo_name() == "Add 10");

    // Undo Add 10
    REQUIRE(stack.undo().is_ok());
    CHECK(state == 0);
    CHECK(!stack.can_undo());
    CHECK(stack.can_redo());

    // Redo Add 10
    REQUIRE(stack.redo().is_ok());
    CHECK(state == 10);

    // Redo Multiply by 2
    REQUIRE(stack.redo().is_ok());
    CHECK(state == 20);
    CHECK(!stack.can_redo());
}

TEST_CASE("CommandStack branching history drops redo forward chain") {
    CommandStack stack;
    int value = 0;

    auto make_add = [&](int delta) {
        return std::make_unique<LambdaCommand>(
            "Add " + std::to_string(delta),
            [&value, delta]() -> base::Result<void> { value += delta; return base::Result<void>::ok(); },
            [&value, delta]() -> base::Result<void> { value -= delta; return base::Result<void>::ok(); }
        );
    };

    REQUIRE(stack.push_and_execute(make_add(10)).is_ok());
    REQUIRE(stack.push_and_execute(make_add(20)).is_ok());
    CHECK(value == 30);

    REQUIRE(stack.undo().is_ok()); // undo 20
    CHECK(value == 10);
    CHECK(stack.can_redo());

    // Branch history by pushing a new command
    REQUIRE(stack.push_and_execute(make_add(50)).is_ok());
    CHECK(value == 60);
    CHECK(!stack.can_redo()); // Redo for +20 is now dropped
    CHECK(stack.count() == 2);
}

TEST_CASE("CommandStack max history boundary") {
    CommandStack stack(3); // Capacity = 3
    int counter = 0;

    auto make_inc = [&]() {
        return std::make_unique<LambdaCommand>(
            "Inc",
            [&]() -> base::Result<void> { ++counter; return base::Result<void>::ok(); },
            [&]() -> base::Result<void> { --counter; return base::Result<void>::ok(); }
        );
    };

    REQUIRE(stack.push_and_execute(make_inc()).is_ok()); // 1
    REQUIRE(stack.push_and_execute(make_inc()).is_ok()); // 2
    REQUIRE(stack.push_and_execute(make_inc()).is_ok()); // 3
    REQUIRE(stack.push_and_execute(make_inc()).is_ok()); // 4 (evicts 1st)
    CHECK(stack.count() == 3);
    CHECK(counter == 4);

    // Can only undo 3 times
    REQUIRE(stack.undo().is_ok());
    REQUIRE(stack.undo().is_ok());
    REQUIRE(stack.undo().is_ok());
    CHECK(counter == 1);
    CHECK(!stack.can_undo());
}

TEST_CASE("CommandStack clean and dirty state tracking") {
    CommandStack stack;
    int state = 0;

    CHECK(stack.is_clean());

    REQUIRE(stack.push_and_execute(std::make_unique<LambdaCommand>(
        "Edit",
        [&]() -> base::Result<void> { state = 1; return base::Result<void>::ok(); },
        [&]() -> base::Result<void> { state = 0; return base::Result<void>::ok(); }
    )).is_ok());

    CHECK(!stack.is_clean());

    // Mark as clean (e.g., after saving project to disk)
    stack.mark_clean();
    CHECK(stack.is_clean());

    // Undoing past save point makes it dirty
    REQUIRE(stack.undo().is_ok());
    CHECK(!stack.is_clean());

    // Redoing back to save point makes it clean again
    REQUIRE(stack.redo().is_ok());
    CHECK(stack.is_clean());
}

TEST_CASE("CommandStack transaction grouping and atomic undo") {
    CommandStack stack;
    std::string text;

    REQUIRE(stack.begin_transaction("Batch Insert").is_ok());
    CHECK(stack.is_in_transaction());

    REQUIRE(stack.push_and_execute(std::make_unique<LambdaCommand>(
        "Insert Hello",
        [&]() -> base::Result<void> { text += "Hello "; return base::Result<void>::ok(); },
        [&]() -> base::Result<void> { text = text.substr(0, text.size() - 6); return base::Result<void>::ok(); }
    )).is_ok());

    REQUIRE(stack.push_and_execute(std::make_unique<LambdaCommand>(
        "Insert World",
        [&]() -> base::Result<void> { text += "World"; return base::Result<void>::ok(); },
        [&]() -> base::Result<void> { text = text.substr(0, text.size() - 5); return base::Result<void>::ok(); }
    )).is_ok());

    CHECK(text == "Hello World");

    // Commit transaction: merges commands into a single undo step
    REQUIRE(stack.commit_transaction().is_ok());
    CHECK(!stack.is_in_transaction());
    CHECK(stack.count() == 1);
    CHECK(stack.undo_name() == "Batch Insert");

    // Single undo unrolls both insertions
    REQUIRE(stack.undo().is_ok());
    CHECK(text.empty());

    // Redo restores both insertions
    REQUIRE(stack.redo().is_ok());
    CHECK(text == "Hello World");
}

TEST_CASE("CommandStack transaction rollback") {
    CommandStack stack;
    int value = 100;

    REQUIRE(stack.begin_transaction("Failing Action").is_ok());

    REQUIRE(stack.push_and_execute(std::make_unique<LambdaCommand>(
        "Change value",
        [&]() -> base::Result<void> { value = 500; return base::Result<void>::ok(); },
        [&]() -> base::Result<void> { value = 100; return base::Result<void>::ok(); }
    )).is_ok());
    CHECK(value == 500);

    // Rollback discards changes and unrolls
    REQUIRE(stack.rollback_transaction().is_ok());
    CHECK(!stack.is_in_transaction());
    CHECK(value == 100);
    CHECK(stack.count() == 0);
    CHECK(!stack.can_undo());
}
