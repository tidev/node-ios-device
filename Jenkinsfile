#! groovy
library 'pipeline-library'

timestamps {
  node('osx && npm-publish && xcode-8.1') {
    def packageVersion = ''
    def isPR = false
    stage('Checkout') {
      // checkout scm
      // Hack for JENKINS-37658 - see https://support.cloudbees.com/hc/en-us/articles/226122247-How-to-Customize-Checkout-for-Pipeline-Multibranch
      checkout([
        $class: 'GitSCM',
        branches: scm.branches,
        extensions: scm.extensions + [[$class: 'CleanBeforeCheckout']],
        userRemoteConfigs: scm.userRemoteConfigs
      ])

      isPR = env.BRANCH_NAME.startsWith('PR-')
      packageVersion = jsonParse(readFile('package.json'))['version']
      currentBuild.displayName = "#${packageVersion}-${currentBuild.number}"
    }

    nodejs(nodeJSInstallationName: 'node 4.7.3') {
      ansiColor('xterm') {
        timeout(15) {
          stage('Build') {
            // Install yarn if not installed
            if (sh(returnStatus: true, script: 'which yarn') != 0) {
              sh 'npm install -g yarn'
            }
            sh 'yarn install'
            sh './bin/build-all.sh'
            // no unit tests!
            fingerprint 'package.json'
            archiveArtifacts 'binding/**/*'
            // Don't tag PRs
            if (!isPR) {
              pushGitTag(name: packageVersion, message: "See ${env.BUILD_URL} for more information.", force: true)
            }
          } // stage
        } // timeout

        stage('Security') {
          // Clean up and install only production dependencies
          sh 'yarn install --production'

          // Scan for NSP and RetireJS warnings
          sh 'yarn global add nsp'
          sh 'nsp check --output summary --warn-only'

          sh 'yarn global add retire'
          sh 'retire --exitwith 0'

          // TODO Run node-check-updates

          step([$class: 'WarningsPublisher', canComputeNew: false, canResolveRelativePaths: false, consoleParsers: [[parserName: 'Node Security Project Vulnerabilities'], [parserName: 'RetireJS']], defaultEncoding: '', excludePattern: '', healthy: '', includePattern: '', messagesPattern: '', unHealthy: ''])
        } // stage

        stage('Publish') {
          if (!isPR) {
            sh 'npm publish' // This uploads the compiled binaries to s3 for us
            // Trigger appc-cli-wrapper job
            build job: 'appc-cli-wrapper', wait: false
          }
        } // stage

        stage('JIRA') {
          if (!isPR) {
            // update affected tickets, create and release version
            updateJIRA('TIMOB', "node-ios-device ${packageVersion}", scm)
          } // if
        } // stage(JIRA)
      } // ansiColor
    } //nodejs
  } // node
} // timestamps
