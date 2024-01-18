# chap1 - 概述

### notify_one or notify_all

`pthread_cond_signal(&p->q_cnd);` 是用于 POSIX 线程（pthreads）的条件变量的函数调用，
它相当于 C++11 中的 `std::condition_variable` 的 `notify_one` 方法。
当你调用 `pthread_cond_signal`，它将至多唤醒一个正在等待该条件变量的线程。
如果没有线程在等待，调用没有任何效果。如果有多个线程在等待，那么其中一个将被选中唤醒，但是选择哪个线程是不确定的。

如果你想要唤醒等待同一个条件变量的所有线程，你应该使用 `pthread_cond_broadcast` 函数。
这对应于 C++11 中的 `std::condition_variable` 的 `notify_all` 方法。

这里是两个函数的简单例子：

- `pthread_cond_signal` - 唤醒至多一个等待条件变量的线程：

  ```c
  pthread_cond_signal(&cond_var);
  ```

- `pthread_cond_broadcast` - 唤醒所有等待条件变量的线程：

  ```c
  pthread_cond_broadcast(&cond_var);
  ```

在实际应用中，选择使用 `pthread_cond_signal` 还是 `pthread_cond_broadcast` 取决于你的同步需求。
如果只需要一个线程响应条件的变化，通常使用 `pthread_cond_signal` 可以减少上下文切换的开销，提高效率。
如果条件的变化需要所有等待线程都得到通知，那么就应该使用 `pthread_cond_broadcast`。

### sscanf

`sscanf` 函数是 C 标准库中的一个函数，用于从字符串读取格式化的输入。这个函数的原型如下：

```c
int sscanf(const char *str, const char *format, ...);
```

- `str` 是包含要解析的数据的字符串。
- `format` 是一个格式字符串，指定了期望的输入格式和如何解析它。
- `...` 表示一个或多个指针，提供了存储解析出的数据的位置。

在你的例子中：

```c
sscanf(line, "%d %64[^\n]", &seconds, message);
```

- `line` 是包含要解析的数据的输入字符串。
- `"%d %64[^\n]"` 是格式字符串，具体含义如下：
  - `%d` 表示匹配一个十进制整数，并将其存储在通过 `&seconds` 提供的 `int` 变量中。
  - ` `（空格）表示跳过任何空白字符，包括空格、制表符、换行符等。
  - `%64[^\n]` 是一个扫描集，它的意思是：
    - `%[` 表示扫描集的开始。
    - `64` 表示最多读取 64 个字符（这是为了防止缓冲区溢出，是一个安全措施）。
    - `^\n` 表示扫描集包含所有字符直到遇到换行符 `\n`。换句话说，它会读取直到但不包括换行符的所有字符。
    - `]` 表示扫描集的结束。
- `&seconds` 是一个指向 `int` 类型变量的指针，用于存储解析出的整数值。
- `message` 是一个字符数组，将存储解析出的字符串，最多可以存储 64 个字符（包括 null 终止符 `\0`，因此实际上只能存储 63 个可见字符）。

综上所述，这行代码尝试从 `line` 字符串中解析出一个整数和一个字符串。整数被存放在 `seconds` 变量中，
字符串被存放在 `message` 数组中，字符串的最大长度为 63 个字符（为了留出一个字符的空间给字符串结束标志 `\0`）。
如果字符串中途遇到了换行符，换行符之前的部分将被存储在 `message` 中，换行符本身不会被存储。

### waitpid

在 C 语言中，`waitpid` 函数是用来等待或检查子进程状态的函数。以下是 `waitpid` 函数的原型：

```c
pid_t waitpid(pid_t pid, int *status, int options);
```

- `pid` 参数指定要等待的子进程。特定的值有特殊含义：
  - `-1` 表示等待任何子进程，相当于 `wait()` 函数。
  - `>0` 表示等待进程 ID 与此值相等的特定子进程。
- `status` 参数是一个指向 `int` 的指针，用来存储子进程的状态信息。如果对状态不感兴趣，可以设置为 `NULL`。
- `options` 参数用来改变 `waitpid` 函数的行为。常见的选项包括：
  - `WNOHANG`：告诉 `waitpid` 不要阻塞，即使没有子进程退出。如果没有可用的状态信息，`waitpid` 会立即返回 0。
  - `WUNTRACED` 和 `WCONTINUED`：这些选项会让 `waitpid` 返回停止（但没有终止）的子进程的状态，或者由停止状态转为继续（在后台继续运行）的子进程的状态。

在您的代码示例中：

```c
pid = waitpid((pid_t)-1, NULL, WNOHANG);
```

这段代码的含义如下：

- `(pid_t)-1`：等待任何子进程。
- `NULL`：不关心子进程的具体退出状态。
- `WNOHANG`：如果没有子进程退出，`waitpid` 不会阻塞当前进程，而是立即返回。

因此，这行代码会立即检查是否有任何子进程已经退出。如果有子进程已经退出，则返回该子进程的 PID。
如果没有子进程退出，`waitpid` 会立即返回 `0`，而不是等待。
这使得父进程可以继续执行其它任务，而不是无限期地等待子进程结束。
这种非阻塞行为经常用于循环中，以定期检查子进程状态，同时执行其他任务。

#### waitpid 的返回值

`waitpid` 函数的返回值取决于它的调用情况和子进程的状态。以下是 `waitpid` 可能返回的值：

1. **大于 0 (`>0`)**: 如果 `options` 设置了 `WNOHANG`，并且有一个子进程已经结束，`waitpid` 返回该子进程的进程 ID (PID)。

2. **等于 0 (`0`)**: 如果 `options` 设置了 `WNOHANG`，但没有任何子进程结束，`waitpid` 会立即返回 `0`。

3. **小于 0 (`-1`)**: 如果 `waitpid` 调用失败，它会返回 `-1`。失败可能是因为没有任何子进程存在，或者函数调用本身有错误（例如，无效的参数）。如果 `waitpid` 失败，可以检查 `errno` 以获取错误码，这可以提供失败的具体原因。

例如：

- `ECHILD`: 调用进程没有子进程。
- `EINVAL`: 指定了无效的选项。
- `EINTR`: 调用被信号打断。

在你给出的代码片段中：

```c
pid = waitpid((pid_t)-1, NULL, WNOHANG);
```

这里 `waitpid` 被用来非阻塞地检查任何子进程是否已经结束，它能够返回：

- 任何已结束子进程的 PID。
- `0`，如果没有子进程已结束。
- `-1`，如果调用失败，例如因为调用进程没有子进程。

### detach and pthread_self

在 C 语言中，使用 POSIX 线程（也称为 pthreads），`pthread_detach` 函数用于将线程标记为“脱离”(detached) 状态。
脱离状态的线程在终止时会自动回收其资源。这意味着一旦脱离状态的线程结束，
它的线程标识符和系统资源（如栈和线程描述符）可以立即被重用或释放，而不需要其他线程对其调用 `pthread_join` 来回收。

这是 `pthread_detach` 函数的原型：

```c
int pthread_detach(pthread_t thread);
```

- `thread` 参数是要脱离的线程的标识符。

如果成功，`pthread_detach` 函数返回 0。如果失败，会返回一个非零错误码。

`pthread_self` 函数返回调用线程的唯一线程标识符。它不需要参数，并且总是成功。

所以这行代码：

```c
status = pthread_detach(pthread_self());
```

执行以下操作：

1. `pthread_self()` 调用获取当前线程的线程标识符。
2. `pthread_detach(...)` 将这个线程标识符作为参数，要求将当前线程转换为脱离状态。
3. `status` 变量存储 `pthread_detach` 函数的返回值，以便检查操作是否成功。

如果 `status` 是 `0`，表示线程成功转换为脱离状态。如果 `status` 是一个非零值，表示函数调用失败，失败原因通常是因为线程标识符无效或者线程已经处于脱离状态。
