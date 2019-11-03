pipeline {
	agent { node { label 'linux-node' } }
	stages {
		stage ('get code') {
			steps {
				sh 'ls -la'
				sh 'lwip'
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