/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

//random engine
static default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	//Set number of particles
	num_particles = 100;

	//create normal distributions for x, y, and theta
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);

	//resize particles vector
	particles.resize(num_particles);

	//initialize particales with Gaussian noise
	for (int i=0; i<num_particles; i++){
		particles[i].id = i;
		particles[i].x = dist_x(gen);
		particles[i].y = dist_y(gen);
		particles[i].theta = dist_theta(gen);
		particles[i].weight = 1.0;
	}

	is_initialized = true;
	
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	//create normal distribution for sensor noise
	normal_distribution<double> sen_x(0, std_pos[0]);
	normal_distribution<double> sen_y(0, std_pos[1]);
	normal_distribution<double> sen_theta(0, std_pos[2]);

	for (int i=0; i<num_particles; i++) {
		if (fabs(yaw_rate)<0.00001) { //constant velocity
			particles[i].x += velocity * delta_t * cos(particles[i].theta);
			particles[i].y += velocity * delta_t * sin(particles[i].theta);	
		}
		else {
			particles[i].x += velocity / yaw_rate * (sin(particles[i].theta + yaw_rate * delta_t) - sin(particles[i].theta));
			particles[i].y += velocity / yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + yaw_rate * delta_t));
			particles[i].theta += yaw_rate * delta_t;
		}

		//add noise to the particles
		particles[i].x += sen_x(gen);
		particles[i].y += sen_y(gen);
		particles[i].theta += sen_theta(gen);
	}


}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.

	for (auto& obs: observations) {

		double min_dist = numeric_limits<double>::max();
		
		for (const auto& pre: predicted) {

			double distance = dist(obs.x, obs.y, pre.x, pre.y);

			//find the closest predicted measurement
			if (distance < min_dist) {
				min_dist = distance;
				obs.id = pre.id;
			}
		}
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	for (auto& p: particles){
    		p.weight = 1.0;

    		//find landmarks in sensor range
    		vector<LandmarkObs> predictions;
   	 	for(const auto& l: map_landmarks.landmark_list){
      			double distance = dist(p.x, p.y, l.x_f, l.y_f);
      			if( distance < sensor_range){ 
        			predictions.push_back(LandmarkObs{l.id_i, l.x_f, l.y_f});
      			}
    		}

    		//transform observations coordinates from vehicle to map
    		vector<LandmarkObs> transformed;
    		for(const auto& obs: observations){
      			double t_x = obs.x * cos(p.theta) - obs.y * sin(p.theta) + p.x;
      			double t_y = obs.x * sin(p.theta) + obs.y * cos(p.theta) + p.y;
      			transformed.push_back(LandmarkObs{obs.id, t_x, t_y});
    		}

    		//associate landmark in range with each observation
    		dataAssociation(predictions, transformed);

    		//update particle's weight with Multivariate Gaussian 
    		for(const auto& t: transformed){

      			Map::single_landmark_s landmark = map_landmarks.landmark_list.at(t.id-1);
			double l_x = landmark.x_f;
			double l_y = landmark.y_f;

			double std_x = std_landmark[0];
			double std_y = std_landmark[1];
			double a = pow((l_x - t.x), 2) / (2 * pow(std_x, 2));
			double b = pow((l_y - t.y), 2) / (2 * pow(std_y, 2));
			double w = exp (-(a+b)) / (2 * M_PI * std_x * std_y); 


      			p.weight *= w;
    		}

    		weights.push_back(p.weight);
  	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	vector<Particle> new_particles;
	new_particles.resize(num_particles);

	discrete_distribution<int> d(weights.begin(), weights.end());
	int index = d(gen);

	uniform_real_distribution<double> rand(0.0, 1.0);	

	double max_weight = *max_element(weights.begin(), weights.end());
	double beta = 0.0;

	//resampling wheel
	for (int i=0; i<num_particles; i++) {
		beta += rand(gen) * 2.0 * max_weight; 
		while (beta > weights[index]) {
			beta -= weights[index];
			index = (index + 1) % num_particles;
		
		}
		new_particles[i] = particles[index];
		
	}

	particles = new_particles;
	weights.clear();
}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	//Clear the previous associations
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations= associations;
 	particle.sense_x = sense_x;
 	particle.sense_y = sense_y;

 	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
