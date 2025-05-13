import assert from "assert";
import {ChatInputCommandInteraction, User} from "discord.js";
import {UserData, UserEvalJob, UserWeightConfig} from "./data";
import zenithCharmJson from "../misc/zenith_charm_config.json";
import {CharmEvalTask, EvalJobRunner} from "./eval_queue";
import {logger} from "./log";

export type CommandOption = {
    type: string;
    description: string;
    required?: boolean;

} & ({
    type: "string" | "user" | "file";
} | {
    type: "integer";
    min?: number;
    max?: number;
})

export interface SubCommandNode {
    description: string;
    children: Record<string, ExecuteNode>;
}

export interface ExecuteNode {
    description: string;
    options: Record<string, CommandOption>;
    executor: (interaction: ChatInputCommandInteraction, userData: UserData, queue: EvalJobRunner) => Promise<void>;
}

export function isSubCommandNode(node: SubCommandNode | ExecuteNode): node is SubCommandNode {
    return "children" in node;
}

const NAME_TO_ID = new Map(zenithCharmJson.map((x, i) => [x.effectName.toLowerCase().replace(/ /g, "_"), i]));

export async function sendReply(interaction: ChatInputCommandInteraction, msg: string) {
    if (interaction.replied) {
        await interaction.followUp({
            content: msg,
            allowedMentions: {
                parse: [],
            },
        });
    } else if (interaction.deferred) {
        await interaction.editReply({
            content: msg,
            allowedMentions: {
                parse: [],
            },
        });

    } else {
        await interaction.reply({
            content: msg,
            allowedMentions: {
                parse: [],
            },
        });
    }
}

async function validateConfigName(interaction: ChatInputCommandInteraction, name: string): Promise<boolean> {
    if (!/^\w+$/.test(name)) {
        await sendReply(interaction, `Name ${name} is invalid, must be non-empty and consist only of letters or underscores.`);
        return false;
    }

    return true;

}

async function validateAndNormalizeConfig(interaction: ChatInputCommandInteraction, config: UserWeightConfig): Promise<UserWeightConfig> {
    const unknowns = Object.entries(config.weights).filter(([key]) => !NAME_TO_ID.has(key));

    if (unknowns.length === 0) {
        return config;
    }

    await sendReply(
        interaction,
        `Unknown weights [${unknowns.map(x => `\`${x}\``).join(", ")}] found in config, deleting (most likely because they were removed in a Monumenta update)!`
    );

    return {
        weights: Object.fromEntries(Object.entries(config.weights).filter(([key]) => NAME_TO_ID.has(key))),
    };
}

async function validateAscii(interaction: ChatInputCommandInteraction, str: string) {
    if (!/^[\x32-\x7e]*$/m.test(str)) {
        logger.audit("command.generic.error.err_not_ascii", {interaction: interaction.id, });
        await sendReply(interaction, "Invalid input file.");
        return false;
    }

    return true;
}

function validateWeight(weight: string) {
    try {
        const value = BigInt(weight);

        if (value > 2000000000n || value < -2000000000n) {
            return false;
        }
    } catch {
        return false;
    }

    return true;
}

async function tryFetchConfig(interaction: ChatInputCommandInteraction, userData: UserData, owner: string | User, name: string): Promise<UserWeightConfig | undefined> {
    const id = typeof owner === "string" ? owner : owner.id;
    const config = userData.fetchConfig(id, name);

    if (config === undefined) {
        await sendReply(interaction, `Config <@${id}>:${name} not found.`);
    }

    return config;
}

async function doEvaluateCharms(interaction: ChatInputCommandInteraction, userData: UserData, queue: EvalJobRunner, task: UserEvalJob) {
    const {owner, name, charmDataUrl: url, cp, } = task;

    const config = await tryFetchConfig(interaction, userData, owner, name);

    if (config === undefined) {
        return;
    }

    await interaction.deferReply();

    // fetch charm data 
    const fetchResult = await fetch(url);

    if (cp > 15 || cp < 1) {
        await sendReply(interaction, `Bad value for charm power ${cp}`);
        return;
    }

    if (!fetchResult.ok) {
        await sendReply(interaction, `Failed to fetch charm data: ${fetchResult.statusText}`);
        return;
    }

    const charmText = await fetchResult.text();

    // validate charmText 
    if (!await validateAscii(interaction, charmText)) {
        return;
    }

    const charmLines = charmText.split("\n")
        .map((str, idx) => [str.trim(), idx + 1] as [string, number])
        .filter(([str]) => str.length !== 0);

    const badLines = charmLines.filter(([str]) => {
        const parts = str.split(";");
        if (parts.length !== 5 && parts.length !== 6) {
            return true;
        }

        const [rarity, , cp, effectNames, effectValues] = parts;

        const effectNameSplit = effectNames.split(":");

        if (effectNameSplit.filter(x => !NAME_TO_ID.has(x)).length > 0) {
            return true;
        }

        const findEffectValueErrors = (content: string): boolean => {
            const data = content.split(":");
            if (data.length !== effectNameSplit.length) {
                return true;
            }

            return data.filter(x => Number.isNaN(parseFloat(x))).length > 0;
        };

        if (!(parseInt(rarity) >= 0 && parseInt(rarity) < 5)) {
            return true;
        }

        if (!(parseInt(cp) > 0)) {
            return true;
        }

        if (findEffectValueErrors(effectValues)) {
            return true;
        }

        if (parts.length === 6) {
            if (findEffectValueErrors(parts[5])) {
                return true;
            }
        }
    });

    const charms = charmLines.map(([a]) => a);

    if (badLines.length > 0) {
        await sendReply(interaction, `Bad charm data on lines: ${badLines.map(([, v]) => v).join(", ")}!`);
        return;
    }

    const result = await queue.evaluate(interaction.user.id, task.owner, task.name, config.weights, charms, cp);

    if (!result.success) {
        await sendReply(interaction, result.error);
        return;
    }

    const names = charms.flatMap(x => {
        const parts = x.split(";");
        const name = parts[1];
        return parts.length === 6 ? [name, `${name} (u)`] : [name];
    });

    await sendReply(
        interaction,
        `Weight: ${result.weight}\n` +
        "```\n" +
        result.charms.map(x => names[x]).join("\n") + "\n" +
        "```"
    );
}

export const COMMANDS: Record<string, SubCommandNode | ExecuteNode> = {
    findeffect: {
        description: "Queries charm effects.",
        options: {
            query: {
                type: "string",
                description: "The query",
            },
        },
        async executor(interaction) {
            const query = interaction.options.getString("query", true);
            const parts = query.split(/\s+/).map(part => part.replace(/\W+/g, "").toLowerCase());

            const results = [...NAME_TO_ID.keys()].filter(effectName => parts.filter(part => effectName.indexOf(part) === -1).length === 0).join("\n");
            if (results.length === 0) {
                await sendReply(interaction, `No results found for query "${query}"`);
                return;
            }

            await sendReply(interaction, "Effects:```\n" + results + "\n```");
        },
    },
    evaluate: {
        description: "Evaluates charms.",
        options: {
            name: {
                type: "string",
                description: "The name of the config",
            },
            data: {
                type: "file",
                description: "The charm data file",
            },
            cp: {
                type: "integer",
                description: "The charm power",
                min: 1,
                max: 15,
                required: false,
            },
            owner: {
                type: "user",
                description: "The user that holds the config, by default the sender",
                required: false,
            },
        },
        async executor(interaction, userData, queue) {
            const name = interaction.options.getString("name", true);
            const charmDataUrl = interaction.options.getAttachment("data", true).url;
            const cp = interaction.options.getInteger("cp") ?? 15;
            const owner = interaction.options.getUser("owner") ?? interaction.user;

            const job: UserEvalJob = {cp, name, owner: owner.id, charmDataUrl, };

            userData.setLatestJob(interaction.user.id, job);
            await doEvaluateCharms(interaction, userData, queue, job);
        },
    },
    rerun: {
        description: "Re-run the latest evaluate task.",
        options: {},
        async executor(interaction, userData, queue) {
            const lastJob = userData.getLatestJob(interaction.user.id);

            if (lastJob === undefined) {
                await sendReply(interaction, "No past job for user.");
                return;
            }

            await doEvaluateCharms(interaction, userData, queue, lastJob);
        },
    },
    queue: {
        description: "Examines the task queue.",
        options: {},
        async executor(interaction, _, queue) {
            function formatTask(task: CharmEvalTask) {
                const common = `<t:${Math.round(task.creationTime / 1000)}:R> <@${task.creator}>: ` +
                    `<@${task.configOwner}>:${task.configName}, ${task.charms.length} charms, ${task.cp} charm power`;
                if ("startTime" in task && typeof task.startTime === "number") {
                    return `${common} (started <t:${Math.round(task.startTime / 1000)}:R>)`;
                }

                return common;
            }

            const currTask = queue.getCurrTask();
            await sendReply(
                interaction,
                (currTask === undefined ?
                    "No current task" :
                    `Current task: ${formatTask(currTask)}`) + "\n" +
                queue.getEntries().map((task, index) => `#${index + 1}: ${formatTask(task)}`).join("\n")
            );
        },
    },
    help: {
        description: "Documentation.",
        options: {},
        async executor(interaction) {
            await sendReply(interaction, `
            ## Charm Eval Bot Help Manual

            **Commands:**

            **/findeffect** - Query charm effects by name.  
            Usage: \`/findeffect query:<text>\`  
            - \`query\`: Space-separated terms to search in effect names.
            You can use this to get effect names for making the config; see \`/config create\`

            **/evaluate** - Evaluate charms using a config.  
            Usage: \`/evaluate name:<config> data:<file> [cp:<1-15>] [owner:<user>]\`  
            - \`name\`: Config name.  
            - \`data\`: Charm data file.  
            - \`cp\`: Charm power (1-15, default 15).  
            - \`owner\`: Config owner (defaults to you).
            The config should be created by \`/config create\`. The charm data file should be obtained via flowey mod (version 1.8 or above).
            With the mod, you can hover over charms and press \`i\`. After you add all charms, run \`/fma copycharmdata\`, and paste the result into a file.
            Then you should upload that file as the attachment for \`data\`.

            **/rerun** - Re-run your last evaluation.  
            Usage: \`/rerun\`

            **/queue** - View current evaluation queue.  
            Usage: \`/queue\`
            `.split("\n").map(x => x.trim()).join("\n"));

            await sendReply(interaction, ` 
            **/config** - Manage configurations.  
            Subcommands:  
            - **get**: View a config.  
              Usage: \`/config get name:<name> [owner:<user>]\`  
            - **delete**: Delete a config.  
              Usage: \`/config delete name:<name> [owner:<user>]\`  
              *Requires permission if modifying another user's config.*  
            - **fork**: Copy a config.  
              Usage: \`/config fork target_name:<name> source_name:<name> [target_owner:<user>] [source_owner:<user>]\`  
            - **create**: Create a new config.  
              Usage: \`/config create name:<name> [owner:<user>] [data:<file>]\`  
              *File format: Each line \`effect=weight\`, comments with #*, names can be obtained via \`/findeffect\`.  
            - **list**: List user's configs.  
              Usage: \`/config list [owner:<user>]\`  
            - **edit**: Modify a config's weight.  
              Usage: \`/config edit name:<name> effect:<effect> weight:<value> [owner:<user>]\`  
              - \`weight\`: Integer between -2000000000 and 2000000000.

            ## Example Usage 
            First you should create a config via \`/config create\`. For example, if you wanted to create a config for flame charms, you should:
            1. Look up the name of the effect via \`/findeffect\`, so for example you may run \`/findeffect query:meteor cooldown\`
            2. Create the config with \`/config create\`
            3. Use \`/config edit\` with the effect name from step 1 and give it a weight; this assigns the relative importance of an effect.
            4. Obtain charm data via flowey mod.
            5. Use \`/evaluate\`
            `.split("\n").map(x => x.trim()).join("\n"));
        },
    },
    config: {
        description: "Config manipulation commands.",
        children: {
            get: {
                description: "Reads a configuration.",
                options: {
                    name: {
                        type: "string",
                        description: "The name of the config",
                    },
                    owner: {
                        type: "user",
                        description: "The user that holds the config, by default the sender",
                        required: false,
                    },
                },
                async executor(interaction, userData) {
                    const name = interaction.options.getString("name", true);
                    const owner = interaction.options.getUser("owner") ?? interaction.user;

                    if (!await validateConfigName(interaction, name)) {
                        return;
                    }

                    const config = await tryFetchConfig(interaction, userData, owner, name);

                    if (config === undefined) {
                        return;
                    }

                    const entries = Object.entries(config.weights);

                    if (entries.length === 0) {
                        await sendReply(interaction, "Empty config.");
                    } else {
                        await sendReply(
                            interaction,
                            "```\n" + entries.map(([effectId, value]) => {
                                if (!NAME_TO_ID.has(effectId)) {
                                    return `:warning: ${effectId} = ${value}`;
                                } else {
                                    return `${effectId} = ${value}`;
                                }
                            }).join("\n") + "\n```"
                        );
                    }
                },
            },
            delete: {
                description: "Deletes a configuration.",
                options: {
                    name: {
                        type: "string",
                        description: "The name of the config",
                    },
                    owner: {
                        type: "user",
                        description: "The user that holds the config, by default the sender",
                        required: false,
                    },
                },
                async executor(interaction, userData) {
                    const name = interaction.options.getString("name", true);
                    const owner = interaction.options.getUser("owner") ?? interaction.user;

                    if (!await validateConfigName(interaction, name)) {
                        return;
                    }

                    if (interaction.user.id !== owner.id && !userData.isAdmin(interaction.user.id)) {
                        await sendReply(interaction, "You do not have permission to delete this config!");
                        return;
                    }

                    if (!userData.deleteConfig(owner.id, name)) {
                        await sendReply(interaction, `Config ${owner}:${name} does not exist.`);
                    }
                    await sendReply(interaction, `Deleted config ${owner}:${name}.`);
                },
            },
            fork: {
                description: "Copies a configuration from another user.",
                options: {
                    target_name: {
                        type: "string",
                        description: "The name of the copied config",
                    },
                    source_name: {
                        type: "string",
                        description: "The name of the source config to copy from",
                    },
                    target_owner: {
                        type: "user",
                        description: "The user to copy the config to, by default the sender",
                        required: false,
                    },
                    source_owner: {
                        type: "user",
                        description: "The name of the owner of the source config, by default the sender",
                        required: false,
                    },
                },
                async executor(interaction, userData) {
                    const targetName = interaction.options.getString("target_name", true);
                    const targetOwner = interaction.options.getUser("target_owner") ?? interaction.user;
                    const sourceName = interaction.options.getString("source_name", true);
                    const sourceOwner = interaction.options.getUser("source_owner") ?? interaction.user;

                    if (!await validateConfigName(interaction, targetName)) {
                        return;
                    }

                    if (!await validateConfigName(interaction, sourceName)) {
                        return;
                    }

                    const config = userData.fetchConfig(sourceOwner.id, sourceName);

                    if (config === undefined) {
                        await sendReply(interaction, `Config ${sourceOwner}:${sourceName} not found.`);
                        return;
                    }

                    if (interaction.user.id !== targetOwner.id && !userData.isAdmin(interaction.user.id)) {
                        await sendReply(interaction, "You do not have permission to fork a config to this user!");
                        return;
                    }

                    if (!userData.createConfig(targetOwner.id, targetName, await validateAndNormalizeConfig(interaction, config))) {
                        await sendReply(interaction, `Config ${targetOwner}:${targetName} already exists.`);
                        return;
                    }

                    await sendReply(interaction, `Copied ${sourceOwner}:${sourceName} -> ${targetOwner}:${targetName}`);
                },
            },
            create: {
                description: "Creates a configuration.",
                options: {
                    name: {
                        type: "string",
                        description: "The name of the config",
                    },
                    owner: {
                        type: "user",
                        description: "The user that holds the config, by default the sender",
                        required: false,
                    },
                    data: {
                        type: "file",
                        description: "Configuration data",
                        required: false,
                    },
                },
                async executor(interaction, userData) {
                    const name = interaction.options.getString("name", true);
                    const owner = interaction.options.getUser("owner") ?? interaction.user;
                    const attachmentUrl = interaction.options.getAttachment("data")?.url;

                    if (!await validateConfigName(interaction, name)) {
                        return;
                    }

                    let config: UserWeightConfig = {weights: {}, };

                    if (attachmentUrl !== undefined) {
                        const request = await fetch(attachmentUrl);
                        const initialConfig = await request.text();

                        if (!await validateAscii(interaction, initialConfig)) {
                            return;
                        }

                        try {
                            const data = initialConfig.split("\n")
                                .map(line => {
                                    const idx = line.indexOf("#");
                                    return idx === -1 ? line : line.substring(0, idx);
                                })
                                .map(line => line.trim())
                                .filter(line => line.length !== 0)
                                .map(x => x.split("=", 2))
                                .map(([k, v]) => {
                                    assert(validateWeight(v));
                                    return [k.trim(), v.trim()];
                                });

                            config = await validateAndNormalizeConfig(interaction, {weights: Object.fromEntries(data), });
                        } catch {
                            await sendReply(interaction, "Invalid input file.");
                            return;
                        }
                    }

                    if (interaction.user.id !== owner.id && !userData.isAdmin(interaction.user.id)) {
                        await sendReply(interaction, "You do not have permission to create a config to this user!");
                        return;
                    }

                    if (!userData.createConfig(owner.id, name, await validateAndNormalizeConfig(interaction, config))) {
                        await sendReply(interaction, `Config ${owner}:${name} already exists.`);
                        return;
                    }

                    await sendReply(interaction, `Created config ${owner}:${name}.`);
                },
            },
            list: {
                description: "List user configs.",
                options: {
                    owner: {
                        type: "user",
                        description: "The user that holds the config, by default the sender",
                        required: false,
                    },
                },
                async executor(interaction, userData) {
                    const owner = interaction.options.getUser("owner") ?? interaction.user;
                    const configs = userData.fetchConfigs(owner.id);
                    await sendReply(
                        interaction,
                        configs.length === 0 ?
                            `${owner} has no configs.` :
                            `${owner} has the following configs: ${configs.join(", ")}`
                    );
                },
            },
            edit: {
                description: "Edit a configuration field.",
                options: {
                    name: {
                        type: "string",
                        description: "The name of the config",
                    },
                    effect: {
                        type: "string",
                        description: "The effect to change",
                    },
                    weight: {
                        type: "string",
                        description: "The new weight",
                    },
                    owner: {
                        type: "user",
                        description: "The user that holds the config, by default the sender",
                        required: false,
                    },
                },
                async executor(interaction, userData) {
                    const name = interaction.options.getString("name", true);
                    const effect = interaction.options.getString("effect", true);
                    const weight = interaction.options.getString("weight", true);
                    const owner = interaction.options.getUser("owner") ?? interaction.user;

                    if (!await validateConfigName(interaction, name)) {
                        return;
                    }

                    if (!NAME_TO_ID.has(effect)) {
                        await sendReply(interaction, `Unknown effect ${effect}, try using \`/findeffect\`.`);
                        return;
                    }

                    const config = userData.fetchConfig(owner.id, name);

                    if (config === undefined) {
                        await sendReply(interaction, `Config ${owner}:${name} not found.`);
                        return;
                    }

                    if (interaction.user.id !== owner.id && !userData.isAdmin(interaction.user.id)) {
                        await sendReply(interaction, "You do not have permission to create a config to this user!");
                        return;
                    }

                    if (!validateWeight(weight)) {
                        await sendReply(interaction, `Illegal value for weight: ${weight}`);
                        return;
                    }

                    config.weights[effect] = weight;
                    userData.setConfig(owner.id, name, await validateAndNormalizeConfig(interaction, config));

                    await sendReply(
                        interaction,
                        "Successfully edited config. New config:\n```" + Object.entries(config.weights)
                            .map(([effectId, value]) => `${effectId} = ${value}`)
                            .join("\n") + "\n```"
                    );
                },
            },
        },
    },
};

