#! groovy
library 'pipeline-library'

def canPublish = env.BRANCH_NAME == '1_X'

buildNPMPackage {
  labels = 'osx-11 && git && npm-publish'
  projectKey = 'TIMOB'
  npmVersion = '1.7.0' // this is actually the yarn version to use, could be set to 'latest' instead of explicit version too
  artifacts = 'binding/**/*'
  publish = canPublish
}
