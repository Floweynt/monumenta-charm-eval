import {ApplicationCommandOptionType, Client, Events} from "discord.js";
import {EvalJobRunner} from "./eval_queue";
import {COMMANDS, isSubCommandNode, sendReply} from "./command";
import {UserData} from "./data";
import {getConfig} from "./config";
import {logger} from "./log";
import {mkdir} from "fs/promises";
import {Readable} from "stream";
import {createWriteStream} from "fs";
import assert from "assert";
import {ReadableStream} from "stream/web";

const queue = new EvalJobRunner();
const userData = new UserData();
const config = getConfig();
const client = new Client({intents: [], });

await mkdir("audit", {recursive: true, });
await userData.read("data.json");

// start background tasks
queue.begin();
userData.beginAutoSave("data.json");

client.on(Events.InteractionCreate, async (interaction) => {
    if (!interaction.isChatInputCommand()) {
        return;
    }

    logger.audit("event.command_interaction_create", {
        interactionId: interaction.id,
        channelId: interaction.channelId,
        guildId: interaction.guildId,
        senderId: interaction.user.id,
        command: interaction.commandName,
        subcommand: interaction.options.getSubcommand(false) ?? undefined,
        options: Object.fromEntries(interaction.options["_hoistedOptions"].map((opt, i) => {
            if (opt.type === ApplicationCommandOptionType.Subcommand) {
                return [i.toString(), opt.name];
            } else if (opt.type === ApplicationCommandOptionType.String || opt.type === ApplicationCommandOptionType.Integer) {
                return [opt.name, opt.value];
            } else if (opt.type === ApplicationCommandOptionType.Attachment) {
                // dispatch this async - we don't care about result 
                (async () => {
                    try {
                        const res = await fetch(opt.attachment.url);
                        const fileStream = createWriteStream(`audit/${interaction.id}-${opt.name}`, {flags: "w+", });
                        assert(res.body !== null);
                        Readable.fromWeb(res.body as ReadableStream<any>).pipe(fileStream);
                    } catch (e) {
                        logger.error("while logging attachment", e);
                    }
                })();

                return [opt.name, opt.attachment?.url];
            } else if (opt.type === ApplicationCommandOptionType.User) {
                return [opt.name, opt.user?.id];
            }

            throw Error(`unknown type ${opt.type}`);
        })),
    });

    try {
        const commandName = interaction.commandName;
        const commandNode = COMMANDS[commandName];

        if (!commandNode) {
            await interaction.reply({content: "Unknown command", ephemeral: true, });
            return;
        }

        if (isSubCommandNode(commandNode)) {
            const subCommandName = interaction.options.data[0].name;
            const executeNode = commandNode.children[subCommandName];

            if (!executeNode) {
                await interaction.reply({
                    content: `Unknown subcommand: ${subCommandName}`,
                    ephemeral: true,
                });
                return;
            }

            await executeNode.executor(interaction, userData, queue);
        } else {
            await commandNode.executor(interaction, userData, queue);
        }
    } catch (error) {
        logger.error(`Error handling command ${interaction.commandName}:`, error);

        if (!interaction.replied) {
            await sendReply(interaction, "There was an error while executing this command!");
        }
    }
});

await client.login(config.token);
logger.info(`Running as ${client.application?.id}`);
