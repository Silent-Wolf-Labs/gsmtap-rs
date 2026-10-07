export default {
  testEnvironment: 'jsdom',
  roots: ['<rootDir>/tests/ui'],
  transform: {},
  collectCoverageFrom: [
    'static/app.js',
    'static/components/**/*.js',
    'static/controllers/**/*.js',
    'static/services/**/*.js',
    'static/models/conversion/**/*.js',
    '!static/components/package.json',
  ],
  coverageThreshold: {
    global: {
      statements: 80,
      branches: 75,
      functions: 80,
      lines: 80,
    },
  },
};
