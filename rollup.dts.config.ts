import { defineConfig } from 'rollup';
import { dts } from 'rollup-plugin-dts';

export default defineConfig([
	{
		input: './temp/index.d.ts',
		output: {
			file: './dist/index.d.ts',
			format: 'es'
		},
		plugins: [
			dts({
				respectExternal: true
			})
		]
	}
]);