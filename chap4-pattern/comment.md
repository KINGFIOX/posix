# chap4 - 三种工作模式

## pathconf

```cxx
path_max = pathconf(filepath, _PC_PATH_MAX);
```

第二个参数是选项的意思，我这里选择了要查询的是`_PC_PATH_MAX`，
然后返回`filepath`所在的文件系统的 最大路径长度

## 线程属性

`pthread_attr_init` 是 POSIX 线程库中的一个函数，用于初始化线程属性对象。
在多线程编程中，线程属性对象用于设置线程的一些属性，如栈大小、调度策略、是否可分离等。

在你提供的代码行中：

```c
status = pthread_attr_init(&detached_attr);
```

`pthread_attr_init` 函数初始化由 `detached_attr` 指向的线程属性对象。
一旦初始化后，这个属性对象可以被用来设置线程的属性，然后可以与 `pthread_create` 函数一起使用来创建配置了这些属性的线程。

这里的 `status` 变量用于接收 `pthread_attr_init` 的返回值。返回值通常用于检查函数调用是否成功。
如果函数成功执行，它将返回 0；如果失败，它将返回一个错误码。

关于可分离（detached）状态，这是线程的一个属性。
通常，一个线程可以在两种状态下结束：可连接（joinable）状态或可分离（detached）状态。
如果一个线程是可连接的，那么其他线程可以调用 `pthread_join` 来等待这个线程结束，并回收其资源。
相反，如果一个线程是可分离的，那么它结束时会自动释放资源，其他线程不能对其调用 `pthread_join`。

要将线程设置为可分离状态，可以使用 `pthread_attr_setdetachstate` 函数与属性对象一起设置 `PTHREAD_CREATE_DETACHED`。

例如：

```c
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

// 使用 attr 创建一个新线程
pthread_t thread;
pthread_create(&thread, &attr, thread_function, NULL);

// 因为线程是可分离的，无需对它调用 pthread_join
```

在线程使用完属性对象后，应该使用 `pthread_attr_destroy` 函数来销毁这个属性对象，释放其资源：

```c
pthread_attr_destroy(&attr);
```

记住，使用 POSIX 线程库进行多线程编程需要对线程同步和状态管理有透彻的理解，以避免竞争条件、死锁等多线程问题。

这个 detach 有点：守护线程的意思。
也就是：新创建出来的这个线程，不会挂到 父线程上

## 设置 detach

在 POSIX 线程（pthread）编程中，`pthread_attr_setdetachstate` 函数用于设置线程属性对象
(`pthread_attr_t`) 的分离状态（detach state）。
这个属性决定了线程在退出时是否会被自动销毁和回收资源。

函数原型如下：

```c
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
```

- `attr` 是一个指向线程属性对象的指针，这个对象必须已经被 `pthread_attr_init` 函数初始化。
- `detachstate` 可以设置为 `PTHREAD_CREATE_DETACHED` 或 `PTHREAD_CREATE_JOINABLE`。

如果 `detachstate` 设置为 `PTHREAD_CREATE_DETACHED`，
那么使用这个属性创建的线程将会处于“可分离”状态。
在可分离状态下，线程在终止时不需要其他线程来清理资源（比如堆栈和状态信息）。
操作系统会在该线程终止时自动进行清理。

在你的代码行：

```c
status = pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);
```

`pthread_attr_setdetachstate` 函数被调用来将 `detached_attr`
指向的线程属性对象设置为“可分离”状态。`status` 用于接收函数的返回值。
如果函数成功将属性对象的分离状态设置为 `PTHREAD_CREATE_DETACHED`，它会返回 0；
如果出现错误，它会返回一个错误代码。

这个设置通常在创建新线程之前完成。
设置之后，可以用这个属性对象作为 `pthread_create` 函数的参数，
来创建一个新的线程，该线程会在一开始就处于可分离状态。

以下是一个例子，演示如何设置线程属性并创建一个可分离线程：

```c
#include <pthread.h>

void* thread_function(void* arg) {
    // 线程的工作内容
    return NULL;
}

int main() {
    pthread_attr_t attr;
    pthread_t thread;
    int status;

    // 初始化线程属性对象
    status = pthread_attr_init(&attr);
    if (status != 0) {
        // 错误处理
    }

    // 设置线程为可分离状态
    status = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (status != 0) {
        // 错误处理
    }

    // 创建新线程，使用已经设置为可分离状态的属性
    status = pthread_create(&thread, &attr, thread_function, NULL);
    if (status != 0) {
        // 错误处理
    }

    // 销毁线程属性对象
    pthread_attr_destroy(&attr);

    // 因为线程是可分离的，不需要使用 pthread_join 等待它
    // 主线程可以继续其他工作或退出

    return 0;
}
```

在上述代码中，线程属性 `attr` 被设置为可分离的，然后用它来创建一个新线程。
由于线程是可分离的，主线程不需要调用 `pthread_join` 来等待它。
这可以简化线程管理，尤其是在不需要从线程收集返回值的情况下。

## 默认是 joinable

在 POSIX 线程（pthread）的实现中，线程只能是可结合（joinable）或者可分离（detachable）的。
不存在既不是可结合也不是可分离的状态。默认情况下，当线程被创建时，它们是可结合的。

可结合（joinable）线程：

- 当一个可结合线程终止时，它的退出状态会被保存直到另一个线程对其进行回收（通过 `pthread_join` 调用）。
  这允许其他线程获取这个线程的退出状态并进行适当的清理。
- 如果一个可结合线程没有被其他线程回收（即没有其他线程对其调用 `pthread_join`），
  它将变成僵尸线程（zombie），即它已经停止运行但仍占用系统资源。

可分离（detachable）线程：

- 可分离线程在终止时会自动释放其资源，无需其他线程进行回收。
  这意味着一旦线程结束，它的栈空间和线程描述符等资源会被立即回收。
- 不能对可分离线程调用 `pthread_join`，并且一旦线程被设置为可分离，就不能将其改回可结合状态。

要创建一个可分离线程，可以在创建线程之前将线程属性对象
（`pthread_attr_t`）设置为可分离，或者在线程创建后立即调用 `pthread_detach` 函数。

这是默认为可结合状态的原因：它提供了更多的灵活性，因为可以在任何时候决定回收线程的资源，
而且如果需要线程的退出信息，也可以获取到。
如果线程默认为可分离的，那么一旦线程结束，所有信息都会丢失，这可能不是所有应用程序都想要的。
因此，如果你想要一个可分离线程，你需要显式地指定它。
