import { defineConfig, type UserConfig } from 'tsdown';

const config: UserConfig = defineConfig({
	entry: './src/index.ts',
	format: ['es', 'cjs'],
	minify: true,
	platform: 'node',
	tsconfig: './tsconfig.build.json',
});

export default config;
