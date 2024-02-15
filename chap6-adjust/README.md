# chap6 - posix 针对线程的调整

## putchar 与 putchar_unlocked

`putchar` 和 `putchar_unlocked` 函数都用于输出一个字符到标准输出（通常是屏幕）。它们的主要区别在于线程安全性和性能。

1. **`putchar`**：

   - `putchar` 函数是线程安全的，在多线程环境中使用时可以保证字符正确输出，不会因为多线程同时操作而导致输出混乱。`putchar` 内部会对标准输出进行加锁和解锁操作，确保一次只有一个线程能够进行输出。
   - 由于加锁操作，`putchar` 的性能可能会稍低于 `putchar_unlocked`，特别是在高度并发的环境中。

2. **`putchar_unlocked`**：

   - `putchar_unlocked` 函数不是线程安全的。它不执行任何加锁操作，因此在多线程环境中使用时可能导致输出混乱，如果多个线程尝试同时调用 `putchar_unlocked` 输出到标准输出。
   - 由于省略了加锁和解锁操作，`putchar_unlocked` 的执行速度比 `putchar` 快。这在单线程环境中或者当你自己管理输出流的锁定时，可以提高程序的性能。

简而言之，如果你在编写单线程程序，或者已经通过其他方式确保了对标准输出访问的互斥（例如，通过手动控制输出的锁定和解锁），
使用 `putchar_unlocked` 可以获得更好的性能。反之，如果你在编写多线程程序，并且多个线程需要写入标准输出，
应该使用 `putchar` 来避免潜在的竞争条件和数据混乱。

## pthreads 中的 xxx_r

这三个函数都与终端和用户相关的信息查询有关。下面分别为 `getlogin_r`、`ctermid` 和 `ttyname_r` 提供简单的使用示例。

### 1. `getlogin_r` 示例

`getlogin_r` 函数用于安全地获取当前用户的登录名。

```c
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    char username[256]; // 分配足够大的缓冲区
    if (getlogin_r(username, sizeof(username)) == 0) {
        printf("Current login name: %s\n", username);
    } else {
        perror("getlogin_r failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

### 2. `ctermid` 示例

`ctermid` 函数用于获取当前进程控制终端的路径名。

```c
#include <stdio.h>
#include <stdlib.h>

int main() {
    char terminalPath[L_ctermid]; // L_ctermid 是一个宏，表示路径名的最大长度
    if (ctermid(terminalPath) != NULL) {
        printf("Control terminal path: %s\n", terminalPath);
    } else {
        perror("ctermid failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

### 3. `ttyname_r` 示例

`ttyname_r` 函数用于安全地获取指定文件描述符关联的终端设备的名称。

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    char ttyName[256]; // 分配足够大的缓冲区
    int fd = STDIN_FILENO; // 使用标准输入的文件描述符
    if (ttyname_r(fd, ttyName, sizeof(ttyName)) == 0) {
        printf("TTY name for FD %d: %s\n", fd, ttyName);
    } else {
        perror("ttyname_r failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

这些示例展示了如何使用这三个函数来查询有关当前用户和终端的信息。
`getlogin_r` 获取当前用户登录名，`ctermid` 获取控制终端的路径，
而 `ttyname_r` 获取指定文件描述符所关联的终端名称。这些函数在编写涉及用户或终端信息的程序时非常有用。

## 挂起 与 推迟

在多线程编程中，"推迟和恢复"（defer and resume）是一种控制线程执行流的技术，
它允许一个线程暂时挂起另一个线程的执行，并在适当的时候恢复。
虽然 POSIX 线程（pthreads）库本身不直接提供挂起和恢复线程的功能，
`pthread_kill` 函数可以被用来实现这一机制的一部分，特别是与信号配合使用时。

### `pthread_kill` 函数简介

`pthread_kill` 函数用于向同一进程中的另一个线程发送信号。其原型如下：

```c
int pthread_kill(pthread_t thread, int sig);
```

- `thread` 是目标线程的标识符。
- `sig` 是要发送的信号。如果 `sig` 为 0，则不发送信号，但仍会进行错误检查，这可以用来检测线程的有效性。

### 使用 `pthread_kill` 实现推迟和恢复

使用 `pthread_kill` 实现线程的"推迟和恢复"通常涉及到发送特定的信号来控制线程。以下是一个概念性的实现：

1. **定义信号处理器**：首先，定义一个信号处理器函数，该函数将在接收到特定信号时执行。例如，可以使用 `SIGUSR1` 来推迟线程，使用 `SIGUSR2` 来恢复线程。

2. **推迟线程**：通过向目标线程发送 `SIGUSR1` 信号，并在信号处理器中调用某种形式的暂停（比如 `pause` 或通过循环等待某个条件变量），实现线程的推迟。

3. **恢复线程**：当希望恢复线程时，发送 `SIGUSR2` 信号，并在其信号处理器中更改条件，使得原本因 `SIGUSR1` 信号而暂停的线程可以继续执行。

### 示例代码

这里是一个非常基础的示例框架，展示了如何设置信号处理器：

```c
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

volatile sig_atomic_t pause_thread = 0;

void handle_sigusr1(int sig) {
    pause_thread = 1; // 设置标志以推迟线程
}

void handle_sigusr2(int sig) {
    pause_thread = 0; // 清除标志以恢复线程
}

void* thread_function(void* arg) {
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);

    while (1) {
        if (pause_thread) {
            // 实现某种形式的等待，这里为了简单使用 sleep 模拟
            printf("Thread paused\n");
            sleep(1); // 在实际应用中，可能会使用更复杂的等待/通知机制
        } else {
            // 线程正常执行的代码
            printf("Thread running\n");
            sleep(1);
        }
    }
    return NULL;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, NULL, thread_function, NULL);

    // 给线程一些时间执行
    sleep(2);

    // 推迟线程
    pthread_kill(thread, SIGUSR1);

    // 给推迟操作一些时间生效
    sleep(5);

    // 恢复线程
    pthread_kill(thread, SIGUSR2);

    // 继续执行一段时间然后退出
    sleep(5);

    return 0;
}
```

此示例仅用于说明如何使用 `pthread_kill` 实现基本的推迟和恢复机制。
在实际应用中，需要更精细的控制和错误处理，特别是信号处理和线程同步方面。
此外，频繁地挂起和恢复线程可能会导致性能问题，因此这种技术应谨慎使用。

## write 相比 printf 的优点

在多线程环境中，直接使用 `write` 函数输出到标准输出
（`stdout`，文件描述符为 1）相比于使用 `printf` 函数有几个潜在的优点，
尤其是在上述代码片段的上下文中：

### 1. 原子性

- **`write`**：`write` 系统调用通常在写入小于 PIPE_BUF 大小（POSIX 标准保证至少为 512 字节）的数据时，
  能保证操作的原子性。这意味着在这个长度限制内，`write` 调用的输出会被整体处理，中间不会被其他线程的 `write` 调用打断。
- **`printf`**：`printf` 函数最终也会调用到类似 `write` 的系统调用，
  但在此之前，它会执行格式化字符串的操作，并且在多步骤中操作缓冲区。
  这意味着在多线程环境中，如果没有适当的同步机制，`printf`
  的输出可能会被其他线程的输出打断，导致输出混乱。

### 2. 性能

- **`write`**：直接调用 `write` 可以减少一些 `printf` 内部的处理开销，
  如格式化字符串和处理可变参数列表等。对于简单的输出需求，这可能会带来轻微的性能优势。
- **`printf`**：`printf` 提供了强大的格式化功能，但这些功能的实现需要额外的计算，
  这在高性能或高频率调用的场景下可能会成为瓶颈。

### 3. 控制和灵活性

- 使用 `write` 可以直接控制输出到哪个文件描述符，
  这在需要写入不同输出流（如标准输出、标准错误或其他文件）的场景中非常有用。
- `write` 对于实现无缓冲的输出也更直接，有时这在处理实时数据或需要立即反馈的场景中是必需的。

### 结论

尽管在上述代码中使用 `write` 有其优点，但也需要注意，
直接使用 `write` 缺乏 `printf` 提供的格式化能力和易用性。
在选择使用哪个函数时，应考虑实际应用场景的需求，比如是否需要格式化字符串、
是否在多线程环境中且关心输出的原子性，以及性能考虑等。

此外，即使 `write` 能在一定程度上提供原子性保证，
但在复杂的多线程应用中，仍然需要小心设计同步机制，
以确保整个程序的行为是正确和预期的。

## pthread_once 的返回值

`pthread_once` 函数是 POSIX 线程库中的一个同步原语，
用于确保某个函数在程序的执行期间只被调用一次，无论它被多少个线程访问。
这对于初始化只需要进行一次的资源或执行只需一次的设置非常有用，
如单例模式的实现或系统资源的初始化。

### 函数原型

```c
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));
```

- `once_control` 是指向 `pthread_once_t` 类型的指针，
  它是一个由系统提供的同步原语，用于控制 `init_routine` 是否已经被调用。
  在第一次调用 `pthread_once` 时，`once_control` 应该被初始化为 `PTHREAD_ONCE_INIT`。
- `init_routine` 是一个无参数、无返回值的函数指针，指向需要被执行且只执行一次的函数。

### 返回值

`pthread_once` 函数的返回值为 `int` 类型，用于指示函数执行的结果：

- **0**：函数成功执行，`init_routine` 要么已经被成功调用并返回（如果是第一次调用 `pthread_once`），
  要么在之前的某个时刻已经成功完成（如果是后续的调用）。这意味着你可以安全地假设 `init_routine` 执行了初始化工作。
- **非 0 值**：表示发生了错误。根据 POSIX 标准，可能的错误码包括 `EINVAL` 等，具体取决于实现。
  非 0 返回值表示 `init_routine` 的调用过程中遇到了问题，或者 `once_control` 参数无效。

## pthread_sigmask

`pthread_sigmask` 函数用于改变当前线程的信号掩码，这影响着该线程对不同信号的响应方式。
具体来说，它可以用来阻塞或解除阻塞特定的信号，或者替换当前线程的信号掩码。
这个函数是多线程程序中控制信号处理行为的关键工具，特别是当你需要确保信号只被特定线程处理时。

### 函数原型

```c
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);
```

- **`how`**：这个参数指定了如何修改信号掩码。它可以是以下几个值之一：
  - `SIG_BLOCK`：将 `set` 指定的信号添加到当前信号掩码中，即阻塞这些信号。
  - `SIG_UNBLOCK`：从当前信号掩码中移除 `set` 指定的信号，即解除阻塞这些信号。
  - `SIG_SETMASK`：将当前信号掩码替换为 `set` 指定的信号集。
- **`set`**：指向 `sigset_t` 结构的指针，指定了想要阻塞或解除阻塞的信号集合。
- **`oldset`**：如果不是 `NULL`，则 `pthread_sigmask` 会将操作前的信号掩码存储在 `oldset` 指向的位置。
  这允许程序保存当前的信号掩码状态，以便以后恢复。

### 示例用法

```c
sigset_t signal_set;
int status;

sigemptyset(&signal_set); // 初始化信号集合，清空所有信号
sigaddset(&signal_set, SIGINT); // 将 SIGINT 信号加入信号集合

// 阻塞 SIGINT 信号
status = pthread_sigmask(SIG_BLOCK, &signal_set, NULL);
if (status != 0) {
    // 错误处理
}
```

这个示例展示了如何阻塞 `SIGINT`（通常是由 Ctrl+C 触发的中断信号）。
调用 `pthread_sigmask` 并使用 `SIG_BLOCK` 选项后，当前线程将不会接收到 `SIGINT` 信号。
这意味着如果用户按下 Ctrl+C，程序不会像通常那样终止，直到这个信号被解除阻塞。

### 注意事项

- `pthread_sigmask` 只影响调用它的线程的信号掩码。
  这与 `sigprocmask` 不同，后者在多线程程序中的行为是未定义的，因此在多线程环境中应避免使用 `sigprocmask`。
- 使用 `pthread_sigmask` 可以精细控制哪些线程可以处理哪些信号，这对于确保信号处理逻辑正确执行非常重要。
- 在设计使用信号的多线程程序时，合理管理信号掩码对于避免竞态条件和保证程序的可靠性至关重要。

## sig event

这段代码是关于如何配置并初始化一个`sigevent`结构体，
以便在创建 POSIX 定时器时使用。`sigevent`结构体用于指定当定时器到期时应当发生的事件。
下面是每个字段设置的详细解释：

### `sigevent`结构体字段

- `se.sigev_notify`：这个字段用于指定当定时器到期时的通知方式。
  在这个例子中，它被设置为`SIGEV_THREAD`，意味着将通过启动一个新线程的方式来通知应用程序定时器已经到期。
  这是一种常用的做法，因为它允许定时器到期时执行较为复杂的操作，而不仅仅是发送信号。

- `se.sigev_value.sival_ptr`：这个字段提供了一种方式，允许传递一个用户定义的值给当定时器到期时被调用的函数。
  在这个例子中，它被设置为`&timer_id`，即定时器标识符的地址。这允许定时器到期时执行的函数（或线程）知道是哪个定时器触发了它。
  尽管在这段代码中没有直接使用这个值，但它可以用于更复杂的场景，例如当你有多个定时器并且需要区分它们触发的回调时。

- `se.sigev_notify_function`：这个字段指定了一个函数，该函数将在定时器到期时被调用。
  在这个例子中，它被设置为`timer_thread`函数的地址，
  但使用了`reinterpret_cast`来转换函数指针类型以匹配`sigevent`结构体所期望的签名。
  这是必要的转换，因为`sigev_notify_function`期望的是一个接受单个`sigval`类型参数的函数，
  而`timer_thread`的实际签名可能并不完全匹配这个要求。这种转换需要谨慎使用，以确保安全性和正确性。

- `se.sigev_notify_attributes`：这个字段用于指定新线程的属性。
  在这个例子中，它被设置为`NULL`，意味着将使用默认线程属性。
  如果你需要配置特定的线程属性（如线程堆栈大小、调度策略等），
  可以通过这个字段指定一个`pthread_attr_t`类型的属性对象。

### 总结

通过配置这个`sigevent`结构体，你定义了定时器到期时应当发生的动作：
启动一个新线程来执行`timer_thread`函数，其中可以通过`se.sigev_value`传递额外的信息给该函数。
这种方式为处理定时器事件提供了高度的灵活性和控制能力。

## sem_wait（P 操作）、sem_post（V 操作）

### sem_wait - P 操作（Proberen，尝试）

- **功能**：`sem_wait`函数用于对信号量执行 P 操作，即尝试减少信号量的值。
  如果信号量的值大于 0，`sem_wait`会将它减 1 并立即返回，允许执行继续进行。
  如果信号量的值为 0，调用`sem_wait`的线程将被阻塞，直到信号量的值变得大于 0，
  这意味着有资源可用或条件已满足，线程可以继续执行。
- **用途**：这通常用于控制对共享资源的访问，确保在资源被占用时，其他线程等待资源释放。

### sem_post - V 操作（Verhogen，增加）

- **功能**：`sem_post`函数用于对信号量执行 V 操作，即增加信号量的值。
  当信号量的值增加后，如果有线程因为`sem_wait`调用而被阻塞在该信号量上，其中一个线程将被解除阻塞，允许它继续执行。
- **用途**：这通常用于释放资源或指示条件已经满足，允许等待的线程继续执行。

### 总结

- `sem_wait`（P 操作）和`sem_post`（V 操作）是信号量编程中的基本构建块，用于实现线程或进程之间的同步和互斥。
- 它们管理一个计数器，该计数器代表了可用资源的数量或某个条件的状态。
- `sem_wait`用于等待资源变得可用（减少计数器），而`sem_post`用于释放资源或标记操作完成（增加计数器）。
- 这些操作共同支持复杂的同步策略，使得多个线程或进程可以协调地访问共享资源，而不会引起冲突或产生竞态条件。
