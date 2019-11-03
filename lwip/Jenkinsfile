pipeline {
	agent any
	stages {
		stage ('get code') {
			steps {
				sh 'git --version'
			}
		}
		stage ('build') {
			steps {
				sh 'make'
			}
		}
	}
}