# CThreadPool

CThreadPool is a simple yet efficient thread pool library written in C that allows users to manage and utilize a pool of worker threads for executing tasks concurrently. The library is designed to handle job queues, synchronize thread activities, and manage resources dynamically to achieve optimal multithreaded performance.

## Features

- **Thread Pool Management**: Easily create and manage a pool of threads to perform tasks concurrently.
- **Job Queue**: A job queue system to store and manage tasks dynamically.
- **Synchronization**: Mutexes and condition variables are used to synchronize job execution and thread activities.
- **Scalable**: The thread pool size can be adjusted based on the workload.
- **Robust**: Handles thread creation, destruction, and job synchronization efficiently.

## Getting Started

### Prerequisites

Ensure you have a C compiler installed (e.g., `gcc`). The library uses POSIX threads, so it should work on Unix-like systems (Linux, macOS). 

### Building Instructions

To build the library, include `cthreadpool.c` in your project and link against the `pthread` library:

```sh
gcc -o your_program your_program.c cthreadpool.c -lpthread
```

### Build Instructions for CMake Projects

If you have CThreadPool included in your project's source tree, you can add it as a subdirectory in your CMakeLists.txt.

**Directory Structure Example:**

```
your_project/
├── CMakeLists.txt
├── src/
│   ├── main.c
├── cthreadpool/
│   ├── CMakeLists.txt
│   ├── cthreadpool.c
│   ├── cthreadpool.h
```

**Your Project’s `CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 3.9.2)
project(YourProject VERSION 0.1.0 LANGUAGES C)

# Add the CThreadPool subdirectory
add_subdirectory(cthreadpool)

# Add your executable or library
add_executable(your_executable src/main.c)

# Link against the shared or static version of CThreadPool based on your preference
target_link_libraries(your_executable PRIVATE cthreadpool_shared)  # or cthreadpool for static

# Set include directories if required
target_include_directories(your_executable PRIVATE cthreadpool)
```

### Using the Library

1. **Initialize the Thread Pool**

   Create and initialize a thread pool with the desired number of threads.

   ```c
   #include "cthreadpool.h"

   int main() {
       threadpool_t *pool = threadpool_t_init(4); // Initializes a pool with 4 threads.
       // Add work, wait, and destroy pool as needed.
       threadpool_destroy(pool);
       return 0;
   }
   ```

2. **Add Work to the Thread Pool**

   Add tasks to the pool using `threadpool_add_work`. Each task should be defined as a function pointer and an argument.

   ```c
   void *example_task(void *arg) {
       int *num = (int *)arg;
       printf("Processing task with value: %d\n", *num);
       return NULL;
   }

   // Adding work to the pool
   int value = 42;
   threadpool_add_work(pool, example_task, &value);
   ```

3. **Wait for All Tasks to Complete**

   Use `threadpool_wait` to block until all jobs have been completed.

   ```c
   threadpool_wait(pool);
   ```

4. **Destroy the Thread Pool**

   Clean up resources by destroying the thread pool when done.

   ```c
   threadpool_destroy(pool);
   ```

### Example

Here’s a complete example demonstrating the basic usage:

```c
#include <stdio.h>
#include "cthreadpool.h"

void *print_message(void *arg) {
    char *message = (char *)arg;
    printf("%s\n", message);
    return NULL;
}

int main() {
    threadpool_t *pool = threadpool_t_init(4); // Initialize a thread pool with 4 threads

    // Add tasks to the thread pool
    threadpool_add_work(pool, print_message, "Task 1: Hello from thread 1!");
    threadpool_add_work(pool, print_message, "Task 2: Hello from thread 2!");
    threadpool_add_work(pool, print_message, "Task 3: Hello from thread 3!");

    // Wait for all tasks to complete
    threadpool_wait(pool);

    // Clean up
    threadpool_destroy(pool);
    return 0;
}
```

## API Reference

### Thread Pool Management

- **`threadpool_t *threadpool_t_init(int thread_num);`**
  Initializes the thread pool with the specified number of threads.

- **`void threadpool_add_work(const threadpool_t *threadpool, void *(*worker_func)(void *), void *arg);`**
  Adds a new job to the job queue.

- **`void threadpool_wait(threadpool_t *threadpool);`**
  Blocks until all tasks are completed.

- **`void threadpool_destroy(threadpool_t *threadpool);`**
  Cleans up and deallocates the thread pool resources.

### Job Queue Management

- **`jobqueue_t *jobqueue_t_init(void);`**
  Initializes a job queue.

- **`void jobqueue_t_deinit(jobqueue_t *jobqueue);`**
  Deinitializes and cleans up a job queue.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## Contributing

Contributions are welcome! Feel free to submit issues, feature requests, or pull requests.

## Acknowledgments

- This project makes use of POSIX threads (`pthread`) for multithreaded management.
- Inspired by various multithreaded task management solutions.