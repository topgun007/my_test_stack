pipeline {
	agent linux-node
	stages {
		stage ('get code') {
			steps {
				sh 'ls -la'
				sh 'mkdir build'
				sh 'cd build'
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