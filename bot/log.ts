import {inspect} from "util";

export const logger = {
    audit(event: string, args: unknown) {
        console.log(`[audit] ${event}: ${inspect(args, {
            depth: null,
            colors: false,
            maxArrayLength: null,
            maxStringLength: null,
            compact: true,
            breakLength: Infinity,
        })}`);
    },
    info(msg: unknown) {
        console.info(msg);
    },
    error(...args: unknown[]) {
        console.error(args);
    },
};

