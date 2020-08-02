const path = require('path');

module.exports = {
  mode: 'development',
  devtool: 'source-map',
  module: {
    rules: [{
      test: /\.ts$/,
      use: 'ts-loader',
      exclude: /node_modules/,
    }]
  },
  resolve: {
    extensions: ['.ts', '.js'],
  },
  entry: './src/index.ts',
  output: {
    path: path.resolve(__dirname, 'dist'),
    filename: 'bundle.js',
  },
};
