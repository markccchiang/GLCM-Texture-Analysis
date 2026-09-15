// Runs the tasks of analyses and feature maps with a limit on how many run at a time. Each owner (an analysis or a feature
// map) has its own queue; owners take turns, one task each, so a large one does not hold up those started after it.

/**
 * A task calls `release` once its worker is free again (before its own bookkeeping, so that `pending` is already lower
 * when it reports that it finished); the slot is released when the task settles at the latest
 */
export type Task = (release: () => void) => Promise<void>;

export class Scheduler {
  /** Queued tasks per owner; Map order is the order in which owners get their next task */
  private readonly queues = new Map<object, Task[]>();
  private queued = 0;
  private running = 0;

  constructor(private readonly concurrency: number) {}

  /** Number of queued or running tasks of all owners */
  get pending(): number {
    return this.queued + this.running;
  }

  /** Queues tasks of an owner; a task must not throw */
  enqueue(owner: object, tasks: Task[]): void {
    if (tasks.length === 0) {
      return;
    }
    this.queues.set(owner, [...(this.queues.get(owner) ?? []), ...tasks]);
    this.queued += tasks.length;
    this.pump();
  }

  /** Drops the queued tasks of an owner; running tasks finish. Returns the number dropped. */
  drop(owner: object): number {
    const tasks = this.queues.get(owner);
    if (!tasks) {
      return 0;
    }
    this.queued -= tasks.length;
    this.queues.delete(owner);
    return tasks.length;
  }

  /** Starts tasks while workers are free, taking one task from each owner in turn */
  private pump(): void {
    while (this.running < this.concurrency && this.queues.size > 0) {
      const [owner, tasks] = this.queues.entries().next().value as [object, Task[]];
      const task = tasks.shift()!;
      this.queued -= 1;
      // The owner goes to the back of the line, or leaves it when it has no more queued tasks
      this.queues.delete(owner);
      if (tasks.length > 0) {
        this.queues.set(owner, tasks);
      }
      this.running += 1;
      let released = false;
      const release = () => {
        if (!released) {
          released = true;
          this.running -= 1;
        }
      };
      void task(release).finally(() => {
        release();
        this.pump();
      });
    }
  }
}
