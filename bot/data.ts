import assert from "assert";
import {writeFileSync} from "fs";
import {readFile, writeFile} from "fs/promises";
import {logger} from "./log";

export interface UserWeightConfig {
    weights: Record<string, string>;
}

function validateUid(uid: unknown): asserts uid is string {
    assert(typeof uid === "string");
    assert(/^\d+$/.test(uid));
}

export interface UserEvalJob {
    owner: string;
    name: string;
    charmDataUrl: string;
    cp: number;
}

export class UserData {
    private userConfigs = new Map<string, Map<string, UserWeightConfig>>();
    private lastJob = new Map<string, UserEvalJob>();
    private admins = new Set<string>();

    private dirty = false;

    public async read(file: string) {
        let fileData: unknown;

        try {
            fileData = JSON.parse((await readFile(file)).toString("utf8"));
        } catch (err) {
            if (err.code === "ENOENT") {
                return;
            }
            throw err;
        }

        assert(fileData !== null && typeof fileData === "object");

        if ("userConfigs" in fileData) {
            assert(Array.isArray(fileData.userConfigs));
            fileData.userConfigs.forEach(([key, value]) => {
                assert(typeof key === "string");
                const [uid, name] = key.split(":", 2);
                validateUid(uid);
                assert(/^\w+$/.test(name));
                assert(typeof value === "object");

                this.createConfig0(uid, name, {
                    weights: Object.fromEntries(Object.entries(value).map(([k, v]) => {
                        assert(/^\w+$/.test(k));
                        assert(typeof v === "string");
                        BigInt(v); // try parse 
                        return [k, v];
                    })),
                });
            });
        }

        if ("admins" in fileData) {
            assert(Array.isArray(fileData.admins));
            fileData.admins.forEach(k => {
                validateUid(k);
                this.admins.add(k);
            });
        }

        if ("lastJob" in fileData) {
            assert(Array.isArray(fileData.lastJob));
            fileData.lastJob.forEach(([k, v]) => {
                validateUid(k);
                assert(typeof v === "object");
                assert("owner" in v && typeof v.owner === "string");
                assert("name" in v && typeof v.owner === "string");
                assert("charmDataUrl" in v && typeof v.charmDataUrl === "string");
                assert("cp" in v && typeof v.cp === "number");
                this.lastJob.set(k, v);
            });
        }
    }

    private prepareSaveData(): string {
        const flatConfigs = [...this.userConfigs].flatMap(([uid, configs]) =>
            [...configs].map(([name, data]) =>
                [`${uid}:${name}`, Object.fromEntries(Object.entries(data.weights).map(([effect, weight]) => [effect, weight.toString()]))]
            )
        );

        return JSON.stringify({
            userConfigs: flatConfigs,
            admins: [...this.admins],
            lastJob: [...this.lastJob],
        });
    }

    public async beginAutoSave(file: string) {
        process.on("exit", () => {
            logger.info("Saving data before exit...");
            writeFileSync(file, this.prepareSaveData());
        });

        while (true) {
            await new Promise(function (resolve) {
                // save every 10s if dirty 
                setTimeout(resolve, 10000);
            });

            if (this.dirty) {
                logger.info("Auto-saving data...");
                writeFile(file, this.prepareSaveData());
                this.dirty = false;
            }
        }
    }

    public fetchConfig(user: string, name: string): UserWeightConfig | undefined {
        return structuredClone(this.userConfigs.get(user)?.get(name));
    }

    public fetchConfigs(user: string): string[] {
        return [...(this.userConfigs.get(user)?.keys() ?? [])];
    }

    public deleteConfig(user: string, name: string): boolean {
        this.dirty = true;
        const res = this.userConfigs.get(user)?.delete(name) ?? false;

        if (res) {
            logger.audit("data.deleteConfig", {user, name,});
        }

        return res;
    }

    public createConfig0(user: string, name: string, config: UserWeightConfig): boolean {
        let userMap = this.userConfigs.get(user);

        if (userMap === undefined) {
            userMap = new Map();
            this.userConfigs.set(user, userMap);
        }

        if (userMap.has(name)) {
            return false;
        }

        userMap.set(name, config);
        return true;
    }

    public createConfig(user: string, name: string, config: UserWeightConfig): boolean {
        this.dirty = true;

        const res = this.createConfig0(user, name, config);

        if (res) {
            logger.audit("data.createConfig", {user, name,});
        }

        return res;
    }

    public setConfig(user: string, name: string, config: UserWeightConfig) {
        logger.audit("data.setConfig", {user, name,});
        this.dirty = true;
        this.userConfigs.get(user)?.set(name, config);
    }

    public setLatestJob(user: string, job: UserEvalJob) {
        logger.audit("data.setLatestJob", {user, job,});
        this.lastJob.set(user, job);
    }

    public getLatestJob(user: string): UserEvalJob | undefined {
        return this.lastJob.get(user);
    }

    public isAdmin(user: string) {
        return this.admins.has(user);
    }
}
