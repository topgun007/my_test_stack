pipeline {
	agent { node { label 'linux-node' } }
	stages {
		stage ('get code') {
			steps {
				sh './build.sh'
			}
		}
		stage ('build') {
			steps {
				sh 'cmake ..'
				sh 'make'
			}
		}
	}
}