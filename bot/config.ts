import assert from "assert";
import {readFileSync} from "fs";

export interface BotConfig {
    token: string;
    id: string;
    threads: number;
    timeout: number;
}

let config: BotConfig | undefined;

export function getConfig() {
    if (config === undefined) {
        config = JSON.parse(readFileSync("config.json").toString("utf8")) as BotConfig;
        assert(typeof config.token === "string");
        assert(typeof config.id === "string");
    }

    return config;
}
