/** Limits concurrent fetches to the ESP32 HTTP server (single-threaded). */

const MAX_CONCURRENT = 2;
let inFlight = 0;
const waitQueue: Array<() => void> = [];

function pumpQueue(): void {
  while (inFlight < MAX_CONCURRENT && waitQueue.length > 0) {
    const next = waitQueue.shift();
    if (next) next();
  }
}

export function scheduleApi<T>(fn: () => Promise<T>): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const run = () => {
      inFlight++;
      fn()
        .then(resolve, reject)
        .finally(() => {
          inFlight--;
          pumpQueue();
        });
    };
    if (inFlight < MAX_CONCURRENT) run();
    else waitQueue.push(run);
  });
}
