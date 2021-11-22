#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

Engine::~Engine() {
    if (StackBottom) {
        delete[] std::get<0>(idle_ctx->Stack);
        delete idle_ctx;
    }
    while (alive) {
        context *ctx = alive;
        delete[] std::get<0>(alive->Stack);
        delete ctx;
        alive = alive->next;
    }
    while (blocked) {
        context *ctx = blocked;
        delete[] std::get<0>(blocked->Stack);
        delete ctx;
        blocked = blocked->next;
    }
}

void Engine::Store(context &ctx) {
    char c;
    if (&c <= ctx.Low) {
        ctx.Low = &c;
    } else {
        ctx.Hight = &c;
    }
    //    std::cout << "zdghdfgsd" << &c << std::endl;

    std::size_t stack_size = ctx.Hight - ctx.Low;
    if (std::get<1>(ctx.Stack) < stack_size || std::get<1>(ctx.Stack) > 2 * stack_size) {
        delete[] std::get<0>(ctx.Stack);
        std::get<1>(ctx.Stack) = stack_size;
        std::get<0>(ctx.Stack) = new char[stack_size];
    }

    memcpy(std::get<0>(ctx.Stack), ctx.Low, stack_size);
}

void Engine::Restore(context &ctx) {
    char addr;
    if (&addr >= ctx.Low && &addr <= ctx.Hight) {
        Restore(ctx);
    }
    std::size_t stack_size = ctx.Hight - ctx.Low;
    memcpy(ctx.Low, std::get<0>(ctx.Stack), stack_size);
    cur_routine = &ctx;
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (!alive || (cur_routine == alive && !alive->next)) {
        return;
    }
    context *ctx = alive;
    if (ctx && ctx == cur_routine) {
        ctx = ctx->next;
    }
    if (ctx) {
        sched(ctx);
    }
}

void Engine::sched(void *routine_) {
    context *routine = static_cast<context *>(routine_);
    if (routine_ == nullptr) {
        yield();
    }
    if (cur_routine == routine || routine->is_block) {
        return;
    }
    if (cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    Restore(*routine);
}

void Engine::block(void *_coro) {
    context *coro = static_cast<context *>(_coro);
    if (!coro) {
        coro = cur_routine;
    }
    if (coro->is_block) {
        return;
    }
    coro->is_block = true;
    if (alive == coro) {
        alive = alive->next;
    }
    if (coro->prev) {
        coro->prev->next = coro->next;
    }
    if (coro->next) {
        coro->next->prev = coro->prev;
    }
    coro->prev = nullptr;
    coro->next = blocked;
    blocked = coro;
    if (blocked->next) {
        blocked->next->prev = coro;
    }
    if (coro == cur_routine) {
        if (cur_routine != idle_ctx) {
            if (setjmp(cur_routine->Environment) > 0) {
                return;
            }
            Store(*cur_routine);
        }
        Restore(*idle_ctx);
    }
}

void Engine::unblock(void *_coro) {
    context *coro = static_cast<context *>(_coro);
    if (!coro || !coro->is_block) {
        return;
    }
    coro->is_block = false;
    if (blocked == coro) {
        blocked = blocked->next;
    }
    if (coro->prev) {
        coro->prev->next = coro->next;
    }
    if (coro->next) {
        coro->next->prev = coro->prev;
    }

    if (alive) {
        alive = coro;
        alive->next = nullptr;
        alive->prev = nullptr;
    } else {
        coro->prev = nullptr;
        alive->prev = coro;
        coro->next = alive;
        alive = coro;
    }

    /*    coro->prev = nullptr;
        coro->next = alive;
        alive = coro;
        if (alive->next) {
            alive->next->prev = coro;
        }*/
}
} // namespace Coroutine
} // namespace Afina
