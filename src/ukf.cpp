#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  n_x_ = x_.size();
  n_aug_ = n_x_ + 2;
  lambda_ = 3 - n_x_;

  weights_ = VectorXd(2*n_aug_+1);
  // set weights_
  this->set_weights();

  // initial covariance matrix
  P_ = MatrixXd(5, 5);
  H_laser_ = MatrixXd(2,5);
  R_radar_ = MatrixXd(n_z_radar,n_z_radar);
  R_laser_ = MatrixXd(n_z_laser,n_z_laser);


  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.8;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  x_ = VectorXd(5);
  x_ << 0, 0, 0, 0, 0;

  H_laser_ << 1, 0, 0, 0, 0,
              0, 1, 0, 0, 0;

  R_radar_ << std_radr_*std_radr_, 0, 0,
              0, std_radphi_*std_radphi_, 0,
              0, 0,std_radrd_*std_radrd_;

  R_laser_ << std_laspx_*std_laspx_, 0,
              0, std_laspy_*std_laspy_;

  P_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 50, 0, 0,
        0, 0, 0, 50, 0,
        0, 0, 0, 0, 50;


}

UKF::~UKF() {}



/////////////////////////////////////
/// Helper Functions from Lecture ///
/////////////////////////////////////

void UKF::set_weights() {

  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights_
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }
}

void UKF::GenerateSigmaPoints(MatrixXd* Xsig_out, const VectorXd &x, const MatrixXd &P) {


  int n = x.size();

  //create sigma point matrix
  MatrixXd Xsig = MatrixXd(n, 2 * n + 1);

  //calculate square root of P
  MatrixXd A = P.llt().matrixL();

  //set first column of sigma point matrix
  Xsig.col(0)  = x;

  //set remaining sigma points
  for (int i = 0; i < n; i++){
    Xsig.col(i+1)     = x + sqrt(lambda_+n) * A.col(i);
    Xsig.col(i+1+n) = x - sqrt(lambda_+n) * A.col(i);
  }


  *Xsig_out = Xsig;

}

void UKF::AugmentedSigmaPoints() {


  //create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);

  //create augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);

  //create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  //create augmented mean state
  x_aug.head(n_x_) = this->x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  //create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = this->P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  this->GenerateSigmaPoints(&Xsig_aug, x_aug, P_aug);

  //write result
  this->Xsig_aug_ = Xsig_aug;


}




void UKF::SigmaPointPrediction(const double &delta_t) {


  //create matrix with predicted sigma points as columns
  MatrixXd Xsig_pred = MatrixXd(n_x_, 2 * n_aug_ + 1);

  //predict sigma points
  for (int i = 0; i< 2*n_aug_+1; i++){
    //extract values for better readability
    double p_x = this->Xsig_aug_(0,i);
    double p_y = this->Xsig_aug_(1,i);
    double v = this->Xsig_aug_(2,i);
    double yaw = this->Xsig_aug_(3,i);
    double yawd = this->Xsig_aug_(4,i);
    double nu_a = this->Xsig_aug_(5,i);
    double nu_yawdd = this->Xsig_aug_(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred(0,i) = px_p;
    Xsig_pred(1,i) = py_p;
    Xsig_pred(2,i) = v_p;
    Xsig_pred(3,i) = yaw_p;
    Xsig_pred(4,i) = yawd_p;
  }

  //write result
  Xsig_pred_ = Xsig_pred;

}


void UKF::PredictMeanAndCovariance() {



  //create vector for predicted state
  VectorXd x = VectorXd(n_x_);

  //create covariance matrix for prediction
  MatrixXd P = MatrixXd(n_x_, n_x_);


  //predicted state mean
  x.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
    x = x+ weights_(i) * this->Xsig_pred_.col(i);
  }

  //predicted state covariance matrix
  P.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
    // state difference
    VectorXd x_diff = this->Xsig_pred_.col(i) - x;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P = P + weights_(i) * x_diff * x_diff.transpose() ;
  }

  // std::cout << "Predicted state" << std::endl;
  // std::cout << x << std::endl;
  // std::cout << "Predicted covariance matrix" << std::endl;
  // std::cout << P << std::endl;

  //write result
  this->x_ = x;
  this->P_ = P;
}

void UKF::UpdateNIS(const MatrixXd &S, const VectorXd &z_residual) {

  this->_last_NIS = (z_residual.transpose()*S.inverse())*z_residual;

  cout << " NIS : " << this->_last_NIS << endl;

}


void UKF::PredictRadarMeasurement(VectorXd* z_out, MatrixXd* S_out, MatrixXd* Zsig_out) {

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z_radar, 2 * n_aug_ + 1);

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    // extract values for better readibility
    double p_x = this->Xsig_pred_(0,i);
    double p_y = this->Xsig_pred_(1,i);
    double v  = this->Xsig_pred_(2,i);
    double yaw = this->Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig(1,i) = atan2(p_y,p_x);                                 //phi
    Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z_radar);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  //innovation covariance matrix S
  MatrixXd S = MatrixXd(n_z_radar,n_z_radar);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  S = S + R_radar_;


  //print result
  // std::cout << "z_pred: " << std::endl << z_pred << std::endl;
  // std::cout << "S: " << std::endl << S << std::endl;

  //write result
  *z_out = z_pred;
  *Zsig_out = Zsig;
  *S_out = S;
}


void UKF::UpdateRadarState(const VectorXd &z) {


  MatrixXd Zsig = MatrixXd(n_z_radar, 2 * n_aug_ + 1);
  VectorXd z_pred = VectorXd(n_z_radar);
  MatrixXd S = MatrixXd(n_z_radar,n_z_radar);


  //cout <<"\nPredictRadarMeasurement" << endl;
  this->PredictRadarMeasurement(&z_pred, &S, &Zsig);

  MatrixXd Tc = MatrixXd(n_x_, n_z_radar);

  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = this->Xsig_pred_.col(i) - this->x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_diff = z - z_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;



  //update state mean and covariance matrix
  this->x_ = this->x_ + K * z_diff;
  this->P_  = this->P_  - K*S*K.transpose();
 
   // measurement model
  double p_x = this->x_(0);
  double p_y = this->x_(1);
  double v  = this->x_(2);
  double yaw = this->x_(3);
  double v1 = cos(yaw)*v;
  double v2 = sin(yaw)*v;
  z_pred(0) = sqrt(p_x*p_x + p_y*p_y);                        //r
  z_pred(1) = atan2(p_y,p_x);                                 //phi
  z_pred(2) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y); 
  VectorXd z_innovation = z - z_pred;
  this->UpdateNIS(S, z_innovation);


  //print result
  // std::cout << "Updated state x: " << std::endl << this->x_ << std::endl;
  // std::cout << "Updated state covariance P: " << std::endl << this->P_ << std::endl;


}


void UKF::PredictLaserMeasurement(VectorXd* z_out, MatrixXd* S_out, MatrixXd* Zsig_out) {

  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z_laser, 2 * n_aug_ + 1);

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    // extract values for better readibility
    double p_x = this->Xsig_pred_(0,i);
    double p_y = this->Xsig_pred_(1,i);

    // measurement model
    Zsig(0,i) = p_x;
    Zsig(1,i) = p_y;

  }

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z_laser);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  //innovation covariance matrix S
  MatrixXd S = MatrixXd(n_z_laser,n_z_laser);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  S = S + R_laser_;


  //print result
  // std::cout << "z_pred: " << std::endl << z_pred << std::endl;
  // std::cout << "S: " << std::endl << S << std::endl;

  //write result
  *z_out = z_pred;
  *Zsig_out = Zsig;
  *S_out = S;
}


void UKF::UpdateLaserState(const VectorXd &z) {

  //////////////
  // Method 1 //
  //////////////

  // MatrixXd Zsig = MatrixXd(n_z_laser, 2 * n_aug_ + 1);
  // VectorXd z_pred = VectorXd(n_z_laser);
  // MatrixXd S = MatrixXd(n_z_laser,n_z_laser);
  // this->PredictLaserMeasurement(&z_pred, &S, &Zsig);
  // MatrixXd Tc = MatrixXd(n_x_, n_z_laser);
  // //calculate cross correlation matrix
  // Tc.fill(0.0);
  // for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
  //   //residual
  //   VectorXd z_diff = Zsig.col(i) - z_pred;
  //   // state difference
  //   VectorXd x_diff = this->Xsig_pred_.col(i) - this->x_;
  //   Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  // }
  // //Kalman gain K;
  // MatrixXd K = Tc * S.inverse();
  // //residual
  // VectorXd z_diff = z - z_pred;

  // //update state mean and covariance matrix
  // this->x_ = this->x_ + K * z_diff;
  // this->P_  = this->P_  - K*S*K.transpose();

  //////////////
  // Method 2 //
  //////////////

  VectorXd z_pred = this->H_laser_ * this->x_;
  VectorXd y = z - z_pred;
  MatrixXd Ht = this->H_laser_.transpose();
  MatrixXd S = this->H_laser_* this-> P_ * Ht + this->R_laser_;
  MatrixXd Si = S.inverse();
  MatrixXd PHt = P_ * Ht;
  MatrixXd K = PHt * Si;

  //new estimate
  this->x_ = this->x_ + (K * y);
  long x_size = x_.size();
  MatrixXd I = MatrixXd::Identity(x_size, x_size);
  this->P_ = (I - K * this->H_laser_) *this-> P_;


  VectorXd z_innovation = z - this->H_laser_ * this->x_;
  this->UpdateNIS(S, z_innovation);
 

  //print result
  // std::cout << "Updated state x: " << std::endl << this->x_ << std::endl;
  // std::cout << "Updated state covariance P: " << std::endl << this->P_ << std::endl;


}


/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage measurement_pack) {

  /*****************************************************************************
   *  Initialization
   ****************************************************************************/
  if (!is_initialized_) {

    if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */
      cout << "Initializing EKF with RADAR" << endl;
      float rho = measurement_pack.raw_measurements_[0];
      float theta = measurement_pack.raw_measurements_[1];
      float rho_dot = measurement_pack.raw_measurements_[2];
      this->x_(0) = rho*cos(theta);;
      this->x_(1) = rho*sin(theta);
    }
    else if (measurement_pack.sensor_type_ == MeasurementPackage::LASER) {
      /**
      Initialize state.
      */
      cout << "Initializing EKF with LASER" << endl;
      this->x_(0) = measurement_pack.raw_measurements_[0];
      this->x_(1) = measurement_pack.raw_measurements_[1];

    }

    // done initializing, no need to predict or update
    previous_timestamp_ = measurement_pack.timestamp_;
    is_initialized_ = true;
    return;
  }

  /*****************************************************************************
   *  Prediction
   ****************************************************************************/


  float dt = (measurement_pack.timestamp_ - previous_timestamp_) / 1000000.0; //dt - expressed in seconds
  previous_timestamp_ = measurement_pack.timestamp_;


  this->Prediction(dt);

  /*****************************************************************************
   *  Update
   ****************************************************************************/

  if (measurement_pack.sensor_type_ == MeasurementPackage::RADAR) {
    // Radar updates
    this->UpdateRadar(measurement_pack);
  }else if (measurement_pack.sensor_type_ == MeasurementPackage::LASER) {
    // Laser updates
    this->UpdateLidar(measurement_pack);

  }  

  // print the output
  // cout << "x_ = " << ekf_.x_ << endl;
  // cout << "P_ = " << ekf_.P_ << endl;
  //cout << "\n" << endl;

}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(const double &delta_t) {

  // cout <<"\nAugmentedSigmaPoints" << endl;
  this->AugmentedSigmaPoints();
  // cout <<"\nSigmaPointPrediction" << endl;
  this->SigmaPointPrediction(delta_t);
  // cout <<"\nPredictMeanAndCovariance" << endl;
  this->PredictMeanAndCovariance();

}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  if (use_laser_){
    this->UpdateLaserState(meas_package.raw_measurements_);
  }



}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  if (use_radar_){
    // cout <<"\nUpdateRadarState" << endl;
    this->UpdateRadarState(meas_package.raw_measurements_);
    
  }

}
