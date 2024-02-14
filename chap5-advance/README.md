# chap5 - 线程高级编程

## 进程 共享锁

`pthread_mutexattr_setpshared`函数用于设置互斥锁属性对象（`pthread_mutexattr_t`）中的 “进程共享” 属性。
这个属性决定了一个互斥锁是否可以被不同的进程共享。

这个函数的原型通常如下：

```c
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
```

- `attr`参数是一个指向互斥锁属性对象的指针。
- `pshared`参数指定互斥锁是否可以被不同进程之间共享。它可以取以下两个值之一：
  - `PTHREAD_PROCESS_SHARED`：允许互斥锁被不同的进程共享。
  - `PTHREAD_PROCESS_PRIVATE`：互斥锁只能被同一个进程中的线程共享（这是默认的设置）。

使用`pthread_mutexattr_setpshared`函数可以让你创建一个能够在多个进程之间共享的互斥锁。
这对于进程间同步来说是非常有用的。
例如，当你在使用共享内存时，多个进程可能需要访问共享内存区域，
这时候就需要一个进程共享的互斥锁来保证对共享内存的互斥访问。

要使互斥锁能够被不同的进程共享，你需要做两件事：

1. 设置互斥锁属性对象以允许进程间共享（使用`pthread_mutexattr_setpshared`）。
2. 确保互斥锁本身位于可以被多个进程访问的内存区域中，如共享内存。

这种机制允许设计更加复杂的多进程同步方案，有助于实现进程间的协调和数据共享。

## joinable

joinable（默认）

pthread_create 可以得到 tid。

- 如果是 joinable 的：tid 能被用来 与 线程链接并获取他的返回值，或者取消他
- 如果是 detached 的：pthread_create 返回的 tid 不能被使用。然后这个线程类似于 守护进程，
  前台管不着，然后任何资源都会被系统回收

## pthread_exit 主线程 而不是 return 的意义

`thread_attr.cxx`中，`pthread_exit(NULL);`的调用位于`main`函数的最后。
`pthread_exit`函数用于终止调用线程并可以通过其参数返回一个值给那些可能会等待该线程结束的其他线程。
在`main`函数中调用`pthread_exit`的主要意义在于：

1. **允许其他线程继续执行：** 在多线程程序中，当`main`函数返回时，默认情况下整个进程将结束，这意味着所有的线程都会被强制结束，不论它们的执行状态如何。通过在`main`函数的末尾使用`pthread_exit`，`main`线程会结束，但是进程保持运行状态直到所有其他线程都已结束。这对于那些需要在`main`线程退出后继续运行的后台线程非常有用。

2. **避免终止其他线程：** 如果程序中有其他线程正在执行任务，使用`pthread_exit`而非直接从`main`返回可以保证这些线程能够正常完成它们的任务。在这个特定的代码示例中，创建了一个分离状态的线程（由`pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED)`设置）。分离状态的线程一旦完成，其资源会自动释放。使用`pthread_exit`允许这个分离线程有机会运行完成，而不是随着`main`函数的结束而被未完成地终止。

3. **程序的清晰退出：** 在某些情况下，使用`pthread_exit`作为退出点可以使得程序的结构更加清晰，特别是在涉及到多线程同步和资源清理的时候。它提供了一种机制，确保所有线程的资源都被妥善处理，然后再退出程序。

在你的代码中，`pthread_exit(NULL);`确保了`main`函数可以立即返回，而不会终止其他可能还在运行的线程。
这样做是为了确保分离线程有机会完成它的执行，即使`main`函数的执行已经完成。

## pthread_cancel

`pthread_cancel`函数用来请求取消同一进程中的另一个线程。
当你调用`pthread_cancel`时，目标线程并不会立即停止执行；
相反，取消请求会被发送给目标线程，然后该函数立即返回给调用者。
这意味着`pthread_cancel`是异步的：调用它后，你不能立即知道目标线程是否已经停止执行。
目标线程的取消取决于它的取消状态和取消类型，以及它是否到达了一个取消点或者是否显式检查了取消请求。

为了等待一个线程真正结束，可以使用`pthread_join`函数。这个函数会阻塞调用线程，直到指定的线程结束。
如果目标线程由于`pthread_cancel`的调用而结束，
`pthread_join`可以检索到特殊的退出状态`PTHREAD_CANCELED`，表明线程是被取消的。

以下是一个示例，展示了如何使用`pthread_cancel`和`pthread_join`，以及如何确认线程是否被成功取消：

```c
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void* thread_routine(void* arg) {
    // 该线程将执行一个无限循环，直到被取消
    while (1) {
        printf("Thread is running...\n");
        sleep(1);  // 休眠1秒，模拟工作负载，并提供取消点
    }
    return NULL;
}

int main(void) {
    pthread_t thread;
    void* res;

    // 创建一个新线程
    pthread_create(&thread, NULL, thread_routine, NULL);

    sleep(3);  // 让子线程运行一段时间

    // 请求取消线程
    pthread_cancel(thread);

    // 等待线程结束，并获取线程的退出状态
    pthread_join(thread, &res);

    if (res == PTHREAD_CANCELED) {
        printf("Thread was canceled.\n");
    } else {
        printf("Thread was not canceled (unexpected).\n");
    }

    return 0;
}
```

在这个示例中，主线程创建了一个新线程，后者简单地运行一个无限循环。
主线程在等待几秒后请求取消这个新线程，然后通过`pthread_join`等待这个线程实际结束。
如果新线程被成功取消，`pthread_join`会收到`PTHREAD_CANCELED`作为线程的退出状态，主线程随后会打印出相应的消息。

这个程序演示了即使在调用`pthread_cancel`之后，被取消的线程可能仍然需要一些时间来响应取消请求，
特别是如果它正在执行一个长时间的操作或者等待某个事件。
通过`pthread_join`，我们可以同步等待线程的响应和确保它被正确地清理。

在多线程编程中，特别是使用 POSIX 线程（pthreads）时，"取消点"和"异步取消类型"是两个重要的概念，它们与线程取消（即请求终止线程）的机制密切相关。

## 取消点

取消点是线程检查是否收到取消请求（通过`pthread_cancel`发起）并决定是否终止的一个执行点。
如果线程在取消点收到取消请求，它将执行清理操作（如果有注册清理函数的话）并终止执行。
不是所有函数都是取消点，POSIX 标准定义了一系列的标准函数（如`read()`, `write()`, `sleep()`, 等）作为取消点。

这意味着，如果你想在自己的线程中支持取消操作，你需要确保线程执行过程中会调用这些取消点函数，
或者使用`pthread_testcancel`函数显式创建取消点。这样，线程才有机会响应取消请求。

## 异步取消类型集

异步取消类型指的是线程接收到取消请求后的行为模式。
线程的取消类型可以是异步（`PTHREAD_CANCEL_ASYNCHRONOUS`）或者延迟（`PTHREAD_CANCEL_DEFERRED`）。

- **异步取消（`PTHREAD_CANCEL_ASYNCHRONOUS`）**：线程收到取消请求后会立即执行取消动作，无需等待取消点。
  这种模式下，线程取消可能会在任意的执行点发生，这可能导致资源泄漏或者数据不一致，因为线程可能在完成其清理代码之前就被取消了。
- **延迟取消（`PTHREAD_CANCEL_DEFERRED`）**：线程只会在达到取消点时检查取消请求，这是默认的取消类型。
  这种方式允许线程有序地进行资源清理和退出准备。

使用`pthread_setcanceltype`函数可以设置线程的取消类型。
选择适当的取消类型对于保证程序的稳定性和资源正确释放非常重要。
在多数情况下，使用延迟取消是更安全的选择，因为它允许线程在终止前进行适当的清理操作。

## 线程取消

线程取消是一种允许一个线程请求终止另一个线程的机制。在 POSIX 线程（pthreads）库中，
这是通过调用`pthread_cancel`函数实现的。
线程取消的概念涉及多个方面，包括取消请求、取消类型、取消状态、取消点和线程的清理工作。

### 取消请求

当你想终止一个线程时，可以对其发出取消请求，使用`pthread_cancel(pthread_t thread)`函数。
这里的`thread`参数是要被取消线程的线程 ID。发出取消请求后，并不意味着线程会立即终止；
线程的取消行为取决于它的取消设置和当前状态。

### 取消类型

- **延迟取消（`PTHREAD_CANCEL_DEFERRED`）**：这是默认设置。线程不会立即响应取消请求，
  只有在达到取消点时才会检查并响应取消请求。这种方式允许线程在取消前执行必要的清理工作。
- **异步取消（`PTHREAD_CANCEL_ASYNCHRONOUS`）**：线程会在接收到取消请求后尽可能快地终止，
  而不管当前执行的位置。这种方式可能导致资源泄漏或数据不一致，因为线程可能没有机会执行任何清理工作。

通过`pthread_setcanceltype`函数可以设置线程的取消类型。

### 取消状态

线程可以启用或禁用对取消请求的响应。如果线程禁用了取消，那么即使收到取消请求，
它也会继续运行，直到线程重新启用取消。线程的取消状态可以通过`pthread_setcancelstate`函数进行设置。

### 取消点

取消点是线程检查取消请求并决定是否终止的地方。POSIX 定义了一系列的标准函数作为隐式取消点，
如`read()`, `write()`, `sleep()`等。此外，线程可以通过调用`pthread_testcancel`函数显式地设置取消点，
以检查是否存在挂起的取消请求。

### 线程清理

线程可以注册一个或多个清理句柄（cleanup handlers），这些句柄是在线程被取消时执行的函数。
这允许线程在被取消前释放资源和执行其他清理工作。
使用`pthread_cleanup_push`和`pthread_cleanup_pop`函数来注册和注销清理句柄。

### 示例

下面是一个简单的例子，演示了如何使用线程取消和清理句柄：

```c
#include <pthread.h>
#include <stdio.h>

void cleanup(void *arg) {
    printf("Cleanup: %s\n", (char*)arg);
}

void* thread_func(void *arg) {
    pthread_cleanup_push(cleanup, "Resource 1");
    pthread_cleanup_push(cleanup, "Resource 2");

    /* 执行一些工作 */
    printf("Thread is running...\n");
    /* 模拟长时间运行 */
    sleep(2);

    pthread_cleanup_pop(1);  // 1 means execute the cleanup handler
    pthread_cleanup_pop(1);  // 1 means execute the cleanup handler

    return NULL;
}

int main(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, thread_func, NULL);
    sleep(1);  // 让线程运行一会儿
    pthread_cancel(thread);  // 请求取消线程
    pthread_join(thread, NULL);  // 等待线程结束
    return 0;
}
```

这个例子中，线程注册了两个清理句柄，然后执行一些模拟的工作。
主线程在一段时间后请求取消这个线程，如果线程在取消点被取消（例如，`sleep`函数），
它的清理句柄将会被调用，然后线程终止。

## 异步取消

异步取消是线程取消的一种模式，它允许线程在接收到取消请求时立即开始取消过程，而不是等待到下一个取消点。
这意味着线程可以在执行任何非原子操作或处于任何代码位置时被取消，包括那些通常不作为取消点的函数调用中。

### 异步取消的工作原理

当线程的取消类型被设置为`PTHREAD_CANCEL_ASYNCHRONOUS`时，系统会在线程接收到取消请求的那一刻尽可能快地终止该线程。
这与默认的延迟取消（`PTHREAD_CANCEL_DEFERRED`）相反，在延迟取消模式下，线程只会在达到预定义的取消点时检查和响应取消请求。

### 设置异步取消

可以通过`pthread_setcanceltype`函数设置线程的取消类型为异步取消：

```c
int pthread_setcanceltype(int type, int *oldtype);
```

其中`type`参数应该设置为`PTHREAD_CANCEL_ASYNCHRONOUS`以启用异步取消。

### 异步取消的风险

尽管异步取消允许快速响应取消请求，但它带来了一系列潜在的问题和风险，主要是因为线程可能在任意点被取消：

- **资源泄露**：线程可能在为某个资源分配内存后被取消，而没有机会释放该资源。
- **死锁**：如果线程在持有互斥锁时被取消，它可能无法释放锁，导致其他线程无法获得锁而死锁。
- **数据一致性**：线程可能在更新共享数据的中间被取消，留下部分更新的数据，导致数据不一致。

### 使用建议

因为这些潜在的问题，通常建议仅在特定情况下使用异步取消，并且在使用时要非常小心。
在许多情况下，延迟取消提供了更好的控制和安全性，因为它允许线程在取消点进行清理和资源释放操作。
如果选择使用异步取消，开发者需要仔细设计线程的工作流程，确保即使线程在任何时点被取消，
资源也能被正确管理和释放，同时保持数据的一致性。

## 哑元条件等待谓语

在多线程编程中，尤其是使用条件变量进行同步时，"哑元条件等待谓语"（Dummy Condition Wait Predicate）是一个重要概念。
这个概念通常出现在与条件变量（condition variables）相关的等待和信号机制中，特别是在解释如何正确使用条件变量时。

### 条件变量

首先，理解条件变量的基本用法是重要的。条件变量允许线程以无竞争的方式等待某个条件成真。使用条件变量时，通常涉及以下几个步骤：

1. **加锁互斥量（Mutex）**：保护共享数据和条件的一致性。
2. **检查条件**：线程在等待条件变量之前检查关联的条件是否已经满足。
3. **等待条件**：如果条件未满足，线程会等待（阻塞）在条件变量上，直到其他线程通知（signal）或广播（broadcast）条件变量。
4. **重新检查条件**：被唤醒后，线程应再次检查条件，因为可能存在虚假唤醒（spurious wakeup）或其他线程已经改变了条件。
5. **释放互斥锁**：完成对共享数据的操作后，释放互斥锁。

### 哑元条件等待谓语

哑元条件等待谓语实际上指的是，在等待条件变量时，线程检查的条件（或谓语）可能因为某些原因（如虚假唤醒或其他线程的干预）不再为真。
因此，即使线程被唤醒，它也不能假设被唤醒的原因就是条件已经成立。
换句话说，哑元谓语意味着线程被唤醒时，它所等待的条件可能并没有实际满足，这就是为什么线程需要在一个循环中重新检查条件的原因。

### 示例

考虑以下伪代码，演示了使用条件变量的标准模式：

```c
pthread_mutex_lock(&mutex); // 加锁互斥量
while (!condition) { // 循环检查条件
    pthread_cond_wait(&cond, &mutex); // 等待条件变量，自动释放互斥锁，被唤醒时重新加锁
}
// 此时，条件已经满足
doSomething(); // 执行基于条件满足的操作
pthread_mutex_unlock(&mutex); // 释放互斥锁
```

在这个例子中，`while`循环确保了即使存在哑元条件（即线程被虚假唤醒或条件由于其他原因不满足），线程也会继续等待直到条件实际满足。
这就是哑元条件等待谓语的核心概念：线程在被唤醒时，不能单纯依赖唤醒事件假设条件已经满足，而是需要重新检查条件。

## cleanup 函数

```cxx
#define pthread_cleanup_push(func, val) \
   { \
	     struct __darwin_pthread_handler_rec __handler; \
	     pthread_t __self = pthread_self(); \
	     __handler.__routine = func; \
	     __handler.__arg = val; \
	     __handler.__next = __self->__cleanup_stack; \
	     __self->__cleanup_stack = &__handler;

#define pthread_cleanup_pop(execute) \
	     /* Note: 'handler' must be in this same lexical context! */ \
	     __self->__cleanup_stack = __handler.__next; \
	     if (execute) (__handler.__routine)(__handler.__arg); \
   }
```

## 分包线程、承包线程

并行计算中的"承包线程"和"分包线程"概念可以通过一个简单的例子来解释，比如计算一个大数组的元素之和。
在这个例子中，"承包线程"（Master Thread）负责整体的任务管理和分配，
而"分包线程"（Worker Threads）则并行执行子任务，完成具体的计算工作。

### 场景描述

假设有一个包含数百万整数的大数组，目标是计算这些整数的总和。
单线程处理这个任务可能非常耗时，因此我们采用并行计算来加速这个过程。

### 实现步骤

1. **承包线程**：主线程作为承包线程，它负责将大任务分解为多个小任务。
   例如，如果数组有 1,000,000 个元素，而我们有 10 个可用的线程，
   主线程可以将数组分成 10 个部分，每个部分包含 100,000 个元素。

2. **分包线程**：每个分包线程负责计算分配给它的数组段的元素和。

3. **结果合并**：所有分包线程完成计算后，它们的计算结果返回给承包线程，
   承包线程负责将这些结果合并，得到最终的总和。

### 示例代码

以下是一个简化的 C 语言示例，演示了如何使用 POSIX 线程（pthread）来实现上述并行计算：

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 10
#define ARRAY_SIZE 1000000
int array[ARRAY_SIZE];
long long sum[NUM_THREADS] = {0};
int part = 0; // 用于分割任务

void* sum_array(void* arg) {
    int thread_part = part++;
    for (int i = thread_part * (ARRAY_SIZE / NUM_THREADS); i < (thread_part + 1) * (ARRAY_SIZE / NUM_THREADS); i++) {
        sum[thread_part] += array[i];
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    // 初始化数组
    for (int i = 0; i < ARRAY_SIZE; i++) {
        array[i] = i + 1; // 假设数组元素为1到ARRAY_SIZE
    }

    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, sum_array, (void*)NULL);
    }

    // 等待线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 合并结果
    long long total_sum = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_sum += sum[i];
    }

    printf("Total Sum is %lld\n", total_sum);
    return 0;
}
```

在这个例子中：

- **承包线程**是`main`函数中的主线程，负责创建分包线程，并在所有线程完成后合并结果。
- **分包线程**由`sum_array`函数实现，每个线程计算数组的一部分和。
- 使用了全局数组`sum`来存储每个分包线程计算的部分和，最后由主线程合并这些部分和得到总和。

这个例子展示了并行计算的基本模式：将大任务分解成小任务，多个线程并行处理这些小任务，然后合并结果以得到最终结果。
这种模式大大加快了处理速度，特别是在处理大量数据时。

## 线程私有数据

线程的私有数据（Thread-specific data，TSD），也称为线程局部存储（Thread-Local Storage，TLS），
是一种允许不同线程存储和访问各自独立的数据副本的机制。
这意味着即使多个线程执行相同的代码，它们也能拥有自己的私有数据副本，而这些数据在其他线程中是不可见或不可访问的。

### 为什么需要线程的私有数据？

在多线程程序中，全局变量和静态变量是由所有线程共享的。
这意味着，如果一个线程修改了这些变量的值，其他所有线程看到的也将是修改后的值。
这在很多情况下是不可取的，尤其是当你需要保持某些数据与执行线程相关联时。
使用线程的私有数据可以避免这种全局变量的共享问题，使每个线程都有自己的独立数据副本，从而简化线程间的数据隔离和管理。

### 如何使用线程的私有数据？

在 POSIX 线程库中，线程的私有数据可以通过以下几个关键函数来管理：

- `pthread_key_create`：创建一个键（key），用于标识线程特定数据的位置。
  每个键可以被同一进程中的所有线程访问，但各线程可以在与该键关联的数据位置存储不同的数据值。
- `pthread_setspecific`：将键与线程特定的数据值关联起来。每个线程调用这个函数时，可以为相同的键设置不同的值。
- `pthread_getspecific`：根据键获取与当前线程关联的数据值。
- `pthread_key_delete`：删除之前创建的键，释放它的资源。注意，这并不会自动释放与键关联的数据内存，需要开发者自己负责清理。

### 示例

以下是一个简单的示例，演示了如何使用 POSIX 线程的线程特定数据功能：

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

pthread_key_t key;

void cleanup(void *arg) {
    printf("Freeing thread-specific data for thread %ld\n", (long)pthread_self());
    free(arg);
}

void* thread_func(void *arg) {
    int *my_data = malloc(sizeof(int));
    *my_data = (int)arg;

    pthread_setspecific(key, my_data);
    printf("Thread %ld set thread-specific data to %d\n", (long)pthread_self(), *my_data);

    int *value = pthread_getspecific(key);
    printf("Thread %ld got thread-specific data: %d\n", (long)pthread_self(), *value);

    return NULL;
}

int main() {
    pthread_t thread1, thread2;

    pthread_key_create(&key, cleanup);

    pthread_create(&thread1, NULL, thread_func, (void*)1);
    pthread_create(&thread2, NULL, thread_func, (void*)2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_key_delete(key);

    return 0;
}
```

在这个例子中，两个线程使用同一个键`key`，但它们各自存储和访问的数据是独立的。
`pthread_key_create`创建一个键，并为其注册了一个清理函数`cleanup`，
该函数在键被删除或线程退出时调用，用于释放分配的内存。
每个线程通过`pthread_setspecific`为键设置了一个指向整数的指针，
然后通过`pthread_getspecific`获取并打印这个整数值。
这展示了如何在多线程环境中安全地管理和使用线程特定数据。

## pthread_once

`pthread_once`是一个由 POSIX 线程库提供的函数，它确保某个函数在一个程序中只被执行一次。
这对于执行一些只需初始化一次的操作非常有用，比如初始化全局变量、分配内存、打开文件等操作。
使用`pthread_once`可以避免在多线程环境中因为并发执行而导致的竞态条件。

### 函数原型

`pthread_once`函数的原型如下：

```c
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));
```

- `once_control`是指向`pthread_once_t`类型的变量的指针，该变量用于控制`init_routine`函数是否已经被调用。`pthread_once_t`变量应该被初始化为`PTHREAD_ONCE_INIT`。
- `init_routine`是一个无参数、无返回值的函数指针，指向需要被执行一次的初始化函数。

### 使用示例

```c
#include <pthread.h>
#include <stdio.h>

// 定义pthread_once_t变量并初始化
pthread_once_t once = PTHREAD_ONCE_INIT;

// 只需执行一次的初始化函数
void initialize_once(void) {
    // 执行一些初始化操作，比如初始化全局变量
    printf("Initialized.\n");
}

// 线程函数
void* threadFunc(void *arg) {
    // 调用pthread_once确保initialize_once函数只被执行一次
    pthread_once(&once, initialize_once);
    printf("Thread %ld running.\n", (long)arg);
    return NULL;
}

int main() {
    pthread_t threads[3];

    // 创建多个线程
    for (long i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, threadFunc, (void*)i);
    }

    // 等待线程结束
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
```

在这个示例中，即使有多个线程试图执行`initialize_once`函数，`pthread_once`机制确保该函数只被执行一次。
这使得它成为多线程程序中执行初始化代码的理想选择。

### 注意事项

- 确保`pthread_once_t`类型的变量在使用前被初始化为`PTHREAD_ONCE_INIT`。
- `init_routine`函数应该是无参数和无返回值的。
- 使用`pthread_once`可以避免在多线程程序中使用复杂的锁定机制来保证初始化代码只执行一次，从而简化代码并提高效率。

## 优先级倒置

优先级倒置是多任务或多线程环境中一种潜在的问题，它发生在低优先级的任务或线程阻塞了比它高优先级的任务或线程运行的情况下。
这种现象违反了优先级调度的基本原则，即高优先级的任务应该比低优先级的任务先执行。
优先级倒置通常发生在任务或线程共享资源（如互斥锁、信号量等）的情况下。

### 优先级倒置的三种主要形式：

1. **基本优先级倒置**：当一个高优先级任务需要等待一个低优先级任务释放锁定的资源时发生。
   如果低优先级任务被其他中等优先级任务抢占，高优先级任务可能需要无限期地等待。

2. **不受限的优先级倒置**：这种情况更加严重，其中一个高优先级任务被多个低优先级任务依次阻塞，
   因为这些低优先级任务控制了高优先级任务需要的资源。

3. **链式阻塞**：一个高优先级任务被一个低优先级任务阻塞，而这个低优先级任务又被另一个更低优先级的任务阻塞，形成一条阻塞链。

### 解决优先级倒置的策略：

- **优先级继承协议**：当一个低优先级任务获得了一个高优先级任务需要的锁时，
  它临时继承了高优先级任务的优先级，直到它释放锁。这可以减少高优先级任务的等待时间。

- **优先级天花板协议**：设置一个系统范围的优先级天花板（即最高优先级），
  所有访问共享资源的任务在持有资源时都提升到这个优先级。这样可以避免低优先级任务阻塞高优先级任务。

- **锁分配策略**：通过设计来避免共享资源的锁竞争，例如使用无锁编程技术或精细的锁粒度，减少锁的竞争和阻塞。

优先级倒置问题在实时操作系统（RTOS）中尤其重要，因为在这些系统中，任务的响应时间和执行顺序是至关重要的。
通过采用合适的策略和协议，可以有效地避免或减轻优先级倒置带来的影响。

### 优先级倒置的例子

让我们通过一个简化的例子来说明优先级倒置的概念：

假设有三个任务：任务 A、任务 B 和任务 C，它们的优先级从高到低依次是：任务 A > 任务 B > 任务 C。这三个任务共享一个资源，例如一个互斥锁。

1. **任务 C**开始运行，因为它是唯一准备就绪的任务。在其执行过程中，它锁定了一个共享资源（例如，通过一个互斥锁）。

2. **任务 A**，一个高优先级任务，变为就绪状态并尝试运行。很快，它也尝试访问被任务 C 锁定的共享资源。
   由于资源已被锁定，任务 A 不能继续，尽管它拥有最高的优先级。因此，任务 A 进入等待状态，等待共享资源被释放。

3. 在任务 A 等待共享资源期间，**任务 B**（优先级低于任务 A 但高于任务 C）变为就绪状态并开始执行。
   因为任务 B 的优先级高于任务 C，任务调度器让任务 B 运行，进一步推迟了任务 C 释放共享资源的时间。

在这个场景中，高优先级的任务 A 被低优先级的任务 C 间接阻塞，并且这种阻塞被中等优先级的任务 B 所延长。
这就是典型的优先级倒置问题。任务 A，尽管拥有最高的优先级，
却因为一个低优先级任务的行为（和一个中等优先级任务的介入）而无法继续执行。

这个例子中的优先级倒置问题可能导致系统响应时间变长，特别是在实时系统中，这可能导致严重的后果。
使用诸如优先级继承或优先级天花板等机制可以帮助避免这种情况，
确保即使在共享资源使用中，系统的调度也能反映任务的优先级顺序。
