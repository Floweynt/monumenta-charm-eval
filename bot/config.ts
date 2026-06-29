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
        config.token = process.env.BOT_TOKEN ?? config.token;
        config.id = process.env.BOT_ID ?? config.id;
        assert(typeof config.token === "string", "token must be set in config.json or BOT_TOKEN env var");
        assert(typeof config.id === "string", "id must be set in config.json or BOT_ID env var");
    }

    return config;
}
