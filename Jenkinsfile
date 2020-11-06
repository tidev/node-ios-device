#! groovy
library 'pipeline-library'

def canPublish = env.BRANCH_NAME == '1_X'

buildNPMPackage {
  labels = 'osx && git && npm-publish && xcode && macos-feldman'
  downstream = ['../appc-cli']
  projectKey = 'TIMOB'
  npmVersion = '1.7.0' // this is actually the yarn version to use, could be set to 'latest' instead of explicit version too
  artifacts = 'binding/**/*'
  publish = canPublish
}
