import {REST, Routes, SlashCommandBuilder, SlashCommandSubcommandBuilder} from "discord.js";
import {getConfig} from "./config";
import {COMMANDS, CommandOption, isSubCommandNode} from "./command";
import {logger} from "./log";

const config = getConfig();
const rest = new REST().setToken(config.token);

async function deployCommands() {
    try {
        const commandBuilders: SlashCommandBuilder[] = [];

        for (const [commandName, node] of Object.entries(COMMANDS)) {
            const builder = new SlashCommandBuilder()
                .setName(commandName)
                .setDescription(node.description);

            if (isSubCommandNode(node)) {
                for (const [subCommandName, executeNode] of Object.entries(node.children)) {
                    const subcommandBuilder = new SlashCommandSubcommandBuilder()
                        .setName(subCommandName)
                        .setDescription(executeNode.description);
                    addOptionsToBuilder(subcommandBuilder, executeNode.options);
                    builder.addSubcommand(subcommandBuilder);
                }
            } else {
                addOptionsToBuilder(builder, node.options);
            }

            commandBuilders.push(builder);
        }

        const commandsData = commandBuilders.map(cmd => cmd.toJSON());

        logger.info(`Started refreshing ${commandsData.length} application (/) commands.`);

        const data = await rest.put(
            Routes.applicationCommands(config.id),
            {body: commandsData, }
        ) as unknown[];

        logger.info(`Successfully reloaded ${data.length} application (/) commands.`);
    } catch (error) {
        logger.error(error);
    }
}

function addOptionsToBuilder(
    builder: SlashCommandBuilder | SlashCommandSubcommandBuilder,
    options: Record<string, CommandOption>
) {
    for (const [optionName, option] of Object.entries(options)) {
        switch (option.type) {
        case "string":
            builder.addStringOption(opt =>
                opt.setName(optionName)
                    .setDescription(option.description)
                    .setMinLength(1)
                    .setRequired(option.required ?? true)
            );
            break;
        case "integer":
            builder.addIntegerOption(opt => {
                opt.setName(optionName)
                    .setDescription(option.description)
                    .setRequired(option.required ?? true);

                if (option.min) {
                    opt.setMaxValue(option.min);
                }

                if (option.max) {
                    opt.setMaxValue(option.max);
                }

                return opt;
            });
            break;
        case "user":
            builder.addUserOption(opt =>
                opt.setName(optionName)
                    .setDescription(option.description)
                    .setRequired(option.required ?? true)
            );
            break;
        case "file":
            builder.addAttachmentOption(opt =>
                opt.setName(optionName)
                    .setDescription(option.description)
                    .setRequired(option.required ?? true)
            );
            break;
        default:
            throw new Error(`Unsupported option type: ${(option as CommandOption).type}`);
        }
    }
}

deployCommands();
