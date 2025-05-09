export class BlockingQueue<T> {
    private queue: T[];
    private pendingResolves: ((value: T | PromiseLike<T>) => void)[];

    public constructor() {
        this.queue = [];
        this.pendingResolves = [];
    }

    /**
     * Retrieves and removes the head of the queue, waiting if necessary until an element becomes available.
     * @returns A Promise that resolves with the next item in the queue
     */
    public poll(): Promise<T> {
        // If queue has items, return the first one immediately
        if (this.queue.length > 0) {
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            return Promise.resolve(this.queue.shift()!);
        }

        // Otherwise, return a promise that will resolve when an item is pushed
        return new Promise<T>((resolve) => {
            this.pendingResolves.push(resolve);
        });
    }

    /**
     * Adds an item to the queue. If there are pending poll requests, the oldest one will be resolved immediately.
     * @param item The item to add to the queue
     */
    public push(item: T): void {
        if (this.pendingResolves.length > 0) {
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            const resolve = this.pendingResolves.shift()!;
            resolve(item);
        } else {
            this.queue.push(item);
        }
    }

    public entries() {
        return [...this.queue];
    }

    /**
     * Gets the current number of items in the queue
     * @returns The number of items currently in the queue
     */
    public size(): number {
        return this.queue.length;
    }

    /**
     * Checks whether the queue is empty
     * @returns true if the queue is empty, false otherwise
     */
    public isEmpty(): boolean {
        return this.queue.length === 0;
    }

    /**
     * Clears all items from the queue
     */
    public clear(): void {
        this.queue = [];
    }
}
