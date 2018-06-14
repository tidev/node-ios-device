#! groovy
library 'pipeline-librarye'

buildNPMPackage {
  labels = 'osx && git && npm-publish && xcode-9'
  downstream = ['../appc-cli']
  projectKey = 'TIMOB'
  npmVersion = '1.7.0' // this is actually the yarn version to use, could be set to 'latest' instead of explicit version too
  artifacts = 'binding/**/*'
}
