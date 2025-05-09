// @ts-check
import eslint from "@eslint/js";
import tseslint from "typescript-eslint";
import stylisticTs from "@stylistic/eslint-plugin-ts"; // Import the plugin

export default tseslint.config(
    eslint.configs.recommended,
    tseslint.configs.strict,
    tseslint.configs.stylistic,
    {
        plugins: {
            "@stylistic/ts": stylisticTs, // Add the plugin here
        },
        rules: {
            "@typescript-eslint/no-explicit-any": "warn",
            indent: ["error", 4],
            "linebreak-style": [
                "error",
                "unix"
            ],
            quotes: ["error", "double"],
            semi: ["error", "always"],
            "comma-dangle": [
                "error",
                {
                    "arrays": "never",
                    "objects": "always",
                    "functions": "never",
                }
            ],
            "@stylistic/ts/member-delimiter-style": "error", // Correct rule name
            eqeqeq: "warn",
        },
    }
);
