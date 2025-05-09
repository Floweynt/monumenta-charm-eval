import {writeFile} from "node:fs/promises";
import {BlockingQueue} from "./blocking_queue";
import {spawn} from "node:child_process";
import {file as createTmpFile} from "tmp-promise";
import {logger} from "./log";

export type EvalResult = {
    success: true;
    weight: bigint;
    charms: number[];
} | {
    success: false;
    error: string;
}

export interface CharmEvalTask {
    completionHandler: (result: EvalResult) => void;
    creator: string;
    configOwner: string;
    configName: string;
    weights: Record<string, string>;
    charms: string[];
    cp: number;
    creationTime: number;
}

export interface RunningTask extends CharmEvalTask {
    startTime: number;
}

interface SpawnResult {
    success: boolean;
    result: string | number | Error;
    stdout: Buffer;
    stderr: Buffer;
}

async function doTask(task: CharmEvalTask, filePath: string): Promise<EvalResult> {
    await writeFile(filePath, task.charms.join("\n"));

    const spawnResult = await new Promise<SpawnResult>((resolve) => {
        const args = [
            "--bot-mode", "--in", filePath, "--config-charm-power", task.cp.toString(),
            ...Object.entries(task.weights).flatMap(([k, v]) => [`--weight-${k}`, v.toString()])
        ];

        logger.audit("eval_queue.spawn", {args, });

        const proc = spawn("./mtce", args, {
            timeout: 30 * 1000, // TODO: config? 
        });

        let stdout = Buffer.of();
        let stderr = Buffer.of();

        proc.addListener("close", (code, signal) => {
            if (code !== null) {
                resolve({success: code === 0, result: code, stdout, stderr, });
            } else if (signal !== null) {
                resolve({success: false, result: signal, stdout, stderr, });
            }
        });

        proc.addListener("error", error => {
            resolve({success: false, result: error, stdout, stderr, });
        });

        proc.stdout.on("data", (entry) => stdout = Buffer.concat([stdout, entry]));
        proc.stderr.on("data", (entry) => stderr = Buffer.concat([stderr, entry]));
    });

    const auditLogCommon = {
        stdout: spawnResult.stdout.toString("utf8"),
        stderr: spawnResult.stderr.toString("utf8"),
    };

    if (!spawnResult.success) {

        if (typeof spawnResult.result === "number") {
            logger.audit("eval_queue.spawn.error.exit", {code: spawnResult.result, ...auditLogCommon, });
            return {
                success: false,
                error: `Exited with code ${spawnResult.result}.`,
            };
        } else if (typeof spawnResult.result === "string") {
            logger.audit("eval_queue.spawn.error.signal", {signal: spawnResult.result, ...auditLogCommon, });
            return {
                success: false,
                error: spawnResult.result === "SIGTERM" ? "Task timed out (try fewer charms?)." : `Signal received: ${spawnResult.result}, please report.`,
            };
        } else {
            logger.audit("eval_queue.spawn.error", {message: spawnResult.result, ...auditLogCommon, });
            logger.error(spawnResult.result);
            return {
                success: false,
                error: `Failed with error: ${spawnResult.result.message}.`,
            };
        }
    }

    const rawLines = spawnResult.stdout.toString("utf8").trim().split("\n");

    if (rawLines.length < 1) {
        return {
            success: false,
            error: "Internal error: illegal output from charm evaluator",
        };
    }

    try {
        const result = {
            weight: BigInt(rawLines[0]),
            charms: rawLines.slice(1).map(x => parseInt(x)),
        };

        logger.audit("eval_queue.done", {...result, ...auditLogCommon, });

        return {success: true, ...result, };
    } catch {
        return {
            success: false,
            error: "Internal error.",
        };
    }
}

export class EvalJobRunner {
    private queue = new BlockingQueue<CharmEvalTask>();
    private currTask?: RunningTask;

    public async begin() {
        while (true) {
            const task = await this.queue.poll();
            this.currTask = {...task, startTime: Date.now(), };

            const charmsFile = await createTmpFile();

            try {
                task.completionHandler(await doTask(task, charmsFile.path));
            } finally {
                charmsFile.cleanup();
                this.currTask = undefined;
            }
        }
    }

    public evaluate(
        creator: string,
        configOwner: string,
        configName: string,
        weights: Record<string, string>, charms: string[], cp: number
    ): Promise<EvalResult> {
        return new Promise((resolve) => {
            this.queue.push({
                creator, configOwner, configName,
                completionHandler: resolve,
                weights,
                charms,
                cp,
                creationTime: Date.now(),
            });
        });
    }

    public getCurrTask() {
        return this.currTask;
    }

    public getEntries() {
        return this.queue.entries();
    }
}
