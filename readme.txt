The interest is to combine a thread pool with a DAG. A set number of threads will be created.
The main thread will dole out tasks to the others. The tasks must be completed in an order
that satisfies the dependencies in a DAG. So whenever there is an idle thread in the pool,
the main thread will try to assign it a task, if there is an available task whose dependency
has been met.

The current code is a proof of concept and sloppy in some places, with code duplication and
some bad variable names. However it does demonstrate the concept. The function do_tasks() uses
two threads, navigating the constant task data structure to determine which task should be done
next. I will enjoy making a more robust and general implementation in the future, for my own
interest.

This is loosely based on reading the O'Reilly pthreads book. I was particularly interested in
the section about standard models of organizing thread synchronization. In particular, the book
describes the "boss/worker" model where the main thread assigns tasks to other threads. I want
to further refine this code to use a thread pool, and that would require a sychronized queue
for the tasks. Right now, the "boss" thread joins on the two parallel threads in a loop.
