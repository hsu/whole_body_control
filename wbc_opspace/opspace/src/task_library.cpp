/*
 * Whole-Body Control for Human-Centered Robotics http://www.me.utexas.edu/~hcrl/
 *
 * Copyright (c) 2011 University of Texas at Austin. All rights reserved.
 *
 * Authors: Roland Philippsen and Luis Sentis
 *
 * BSD license:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of
 *    contributors to this software may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR THE CONTRIBUTORS TO THIS SOFTWARE BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <opspace/task_library.hpp>
#include <opspace/TypeIOTGCursor.hpp>

using jspace::pretty_print;

namespace opspace {


  PDTask::
  PDTask(std::string const & name)
    : Task(name),
      initialized_(false)
  {
    declareParameter("goalpos", &goalpos_);
    declareParameter("goalvel", &goalvel_);
    declareParameter("kp", &kp_);
    declareParameter("kd", &kd_);
    declareParameter("maxvel", &maxvel_);
  }
  
  
  Status PDTask::
  check(Vector const * param, Vector const & value) const
  {
    if ((param == &kp_) || (param == &kd_) || (param == &maxvel_)) {
      for (size_t ii(0); ii < value.rows(); ++ii) {
	if (0 > value[ii]) {
	  return Status(false, "gains and limits must be >= 0");
	}
      }
    }
    if (initialized_) {
      if ((param == &kp_) || (param == &kd_) || (param == &maxvel_)
	  || (param == &goalpos_) || (param == &goalvel_)) {
	if (goalpos_.rows() != value.rows()) {
	  return Status(false, "invalid dimension");
	}
      }
    }
    return Status();
  }
  
  
  Status PDTask::
  initPDTask(Vector const & initpos,
	     bool allow_scalar_to_vector)
  {
    int const ndim(initpos.rows());
    if (ndim != kp_.rows()) {
      if ((ndim != 1) && (1 == kp_.rows()) && allow_scalar_to_vector) {
	kp_ = kp_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set kp");
      }
    }
    if (ndim != kd_.rows()) {
      if ((ndim != 1) && (1 == kd_.rows()) && allow_scalar_to_vector) {
	kd_ = kd_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set kd");
      }
    }
    if (ndim != maxvel_.rows()) {
      if ((ndim != 1) && (1 == maxvel_.rows()) && allow_scalar_to_vector) {
	maxvel_ = maxvel_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set maxvel");
      }
    }
    
    goalpos_ = initpos;
    goalvel_ = Vector::Zero(ndim);
    initialized_ = true;
    
    Status ok;
    return ok;
  }
  
  
  Status PDTask::
  computePDCommand(Vector const & curpos,
		   Vector const & curvel,
		   Vector & command)
  {
    Status st;
    if ( ! initialized_) {
      st.ok = false;
      st.errstr = "not initialized";
    }
    else {
      command = kp_.cwise() * (goalpos_ - curpos);
      // component-wise velocity saturation, beware of div by zero
      for (int ii(0); ii < command.rows(); ++ii) {
	if ((maxvel_[ii] > 1e-4) && (kd_[ii] > 1e-4)) {
	  double const sat(fabs((command[ii] / maxvel_[ii]) / kd_[ii]));
	  if (sat > 1.0) {
	    command[ii] /= sat;
	  }
	}
      }
      command += kd_.cwise() * (goalvel_ - curvel);
    }
    return st;
  }
  
  
  SelectedJointPostureTask::
  SelectedJointPostureTask(std::string const & name)
    : Task(name),
      kp_(100.0),
      kd_(20.0),
      initialized_(false)
  {
    declareParameter("selection", &selection_);
    declareParameter("kp", &kp_);
    declareParameter("kd", &kd_);
  }
  
  
  Status SelectedJointPostureTask::
  init(Model const & model) {
    size_t const ndof(model.getNDOF());
    active_joints_.clear();	// in case we get called multiple times
    for (size_t ii(0); ii < selection_.rows(); ++ii) {
      if (ii >= ndof) {
	break;
      }
      if (selection_[ii] > 0.5) {
	active_joints_.push_back(ii);
      }
    }
    if (active_joints_.empty()) {
      return Status(false, "no active joints");
    }
    size_t const ndim(active_joints_.size());
    actual_ = Vector::Zero(ndim);
    command_ = Vector::Zero(ndim);
    jacobian_ = Matrix::Zero(ndim, ndof);
    for (size_t ii(0); ii < ndim; ++ii) {
      actual_[ii] = model.getState().position_[active_joints_[ii]];
      jacobian_.coeffRef(ii, active_joints_[ii]) = 1.0;
    }
    initialized_ = true;
    Status ok;
    return ok;
  }
  
  
  Status SelectedJointPostureTask::
  update(Model const & model) { 
    Status st;
    if ( ! initialized_) {
      st.ok = false;
      st.errstr = "not initialized";
      return st;
    }
    Vector vel(actual_.rows());
    for (size_t ii(0); ii < active_joints_.size(); ++ii) {
      actual_[ii] = model.getState().position_[active_joints_[ii]];
      vel[ii] = model.getState().velocity_[active_joints_[ii]];
    }
    command_ = -kp_ * actual_ - kd_ * vel;
    return st;
  }
  
  
  Status SelectedJointPostureTask::
  check(double const * param, double value) const
  {
    Status st;
    if (((&kp_ == param) && (value < 0)) || ((&kd_ == param) && (value < 0))) {
      st.ok = false;
      st.errstr = "gains must be >= 0";
    }
    return st;
  }
  
  
  Status SelectedJointPostureTask::
  check(Vector const * param, Vector const & value) const
  {
    Status st;
    if ((&selection_ == param) && (value.rows() == 0)) {
      st.ok = false;
      st.errstr = "selection must not be empty";
    }
    return st;
  }
  
  
  TrajectoryTask::
  TrajectoryTask(std::string const & name)
    : Task(name),
      cursor_(0),
      dt_seconds_(-1),
      goal_changed_(false)
  {
    declareParameter("dt_seconds", &dt_seconds_);
    declareParameter("goal", &goal_);
    declareParameter("kp", &kp_);
    declareParameter("kd", &kd_);
    declareParameter("maxvel", &maxvel_);
    declareParameter("maxacc", &maxacc_);
  }
  
  
  TrajectoryTask::
  ~TrajectoryTask()
  {
    delete cursor_;
  }
  
  
  Status TrajectoryTask::
  initTrajectoryTask(Vector const & initpos, bool allow_scalar_to_vector)
  {
    if (0 > dt_seconds_) {
      return Status(false, "you did not (correctly) set dt_seconds");
    }
    
    int const ndim(initpos.rows());
    if (ndim != maxvel_.rows()) {
      if ((ndim != 1) && (1 == maxvel_.rows()) && allow_scalar_to_vector) {
	maxvel_ = maxvel_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set maxvel");
      }
    }
    if (ndim != maxacc_.rows()) {
      if ((ndim != 1) && (1 == maxacc_.rows()) && allow_scalar_to_vector) {
	maxacc_ = maxacc_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set maxacc");
      }
    }
    if (ndim != kp_.rows()) {
      if ((ndim != 1) && (1 == kp_.rows()) && allow_scalar_to_vector) {
	kp_ = kp_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set kp");
      }
    }
    if (ndim != kd_.rows()) {
      if ((ndim != 1) && (1 == kd_.rows()) && allow_scalar_to_vector) {
	kd_ = kd_[0] * Vector::Ones(ndim);
      }
      else {
	return Status(false, "you did not (correctly) set kd");
      }
    }
    
    if (cursor_) {
      if (cursor_->dt_seconds_ != dt_seconds_) {
	delete cursor_;
	cursor_ = 0;
      }
    }
    if ( ! cursor_) {
      cursor_ = new TypeIOTGCursor(ndim, dt_seconds_);
    }
    
    goal_ = initpos;
    goal_changed_ = true;
    
    return Status();
  }
  
  
  Status TrajectoryTask::
  computeTrajectoryCommand(Vector const & curpos,
			   Vector const & curvel,
			   Vector & command)
  {
    if ( ! cursor_) {
      return Status(false, "not initialized");
    }
    
    if (goal_changed_) {
      cursor_->position() = curpos;
      cursor_->velocity() = curvel;
      goal_changed_ = false;
    }
    
    if (0 > cursor_->next(maxvel_, maxacc_, goal_)) {
      return Status(false, "trajectory generation error");
    }
    
    //
    // XXXX to do: (see also PDTask) implement PD velocity saturation
    // at maxvel_ (in addition to the velocity-limited trajectory)
    //
    command
      = kp_.cwise() * (cursor_->position() - curpos)
      + kd_.cwise() * (cursor_->velocity() - curvel);
    
    Status ok;
    return ok;
  }
  
  
  Status TrajectoryTask::
  check(double const * param, double value) const
  {
    if ((param == &dt_seconds_) && (0 >= value)) {
      return Status(false, "dt_seconds must be > 0");
    }
    return Status();
  }
  
  
  Status TrajectoryTask::
  check(Vector const * param, Vector const & value) const
  {
    if (cursor_) {
      if (param == &goal_) {
	if (cursor_->ndof_ != value.rows()) {
	  return Status(false, "invalid goal dimension");
	}
	goal_changed_ = true;
      }
      if ((param == &kp_) || (param == &kd_)
	  || (param == &maxvel_) || (param == &maxacc_)) {
	if (cursor_->ndof_ != value.rows()) {
	  return Status(false, "invalid dimension");
	}
	for (size_t ii(0); ii < cursor_->ndof_; ++ii) {
	  if (0 > value[ii]) {
	    return Status(false, "bounds and gains must be >= 0");
	  }
	}
      }
    }
    return Status();
  }
  
  
  void TrajectoryTask::
  dbg(std::ostream & os,
      std::string const & title,
      std::string const & prefix) const
  {
    if ( ! title.empty()) {
      os << title << "\n";
    }
    os << prefix << "trajectory task: `" << name_ << "'\n";
    if ( ! cursor_) {
      os << prefix << "  NOT INITIALIZED\n";
    }
    pretty_print(actual_, os, "actual", prefix + "  ");
    pretty_print(cursor_->position(), os, "carrot", prefix + "  ");
    pretty_print(goal_, os, "goal", prefix + "  ");
  }
  
  
  PositionTask::
  PositionTask(std::string const & name)
    : TrajectoryTask(name),
      end_effector_id_(-1),
      control_point_(Vector::Zero(3))
  {
    declareParameter("end_effector_id", &end_effector_id_);
    declareParameter("control_point", &control_point_);
  }
  
  
  Status PositionTask::
  init(Model const & model)
  {
    if (0 > end_effector_id_) {
      return Status(false, "you did not (correctly) set end_effector_id");
    }
    if (3 != control_point_.rows()) {
      return Status(false, "control_point needs to be three dimensional");
    }
    if (0 == updateActual(model)) {
      return Status(false, "updateActual() failed, did you specify a valid end_effector_id?");
    }
    return initTrajectoryTask(actual_, true);
  }
  
  
  Status PositionTask::
  update(Model const & model)
  {
    taoDNode const * ee_node(updateActual(model));
    if (0 == ee_node) {
      return Status(false, "updateActual() failed, did you specify a valid end_effector_id?");
    }
    
    Matrix Jfull;
    if ( ! model.computeJacobian(ee_node, actual_[0], actual_[1], actual_[2], Jfull)) {
      return Status(false, "failed to compute Jacobian (unsupported joint type?)");
    }
    jacobian_ = Jfull.block(0, 0, 3, Jfull.cols());
    
    return computeTrajectoryCommand(actual_,
				    jacobian_ * model.getState().velocity_,
				    command_);
  }
  
  
  taoDNode const * PositionTask::
  updateActual(Model const & model)
  {
    taoDNode * ee_node(model.getNode(end_effector_id_));
    if (ee_node) {
      jspace::Transform ee_transform;
      model.computeGlobalFrame(ee_node,
			       control_point_[0],
			       control_point_[1],
			       control_point_[2],
			       ee_transform);
      actual_ = ee_transform.translation();
    }
    return ee_node;
  }
  
  
  PostureTask::
  PostureTask(std::string const & name)
    : TrajectoryTask(name)
  {
  }
  
  
  Status PostureTask::
  init(Model const & model)
  {
    jacobian_ = Matrix::Identity(model.getNDOF(), model.getNDOF());
    return initTrajectoryTask(model.getState().position_, true);
  }
  
  
  Status PostureTask::
  update(Model const & model)
  {
    actual_ = model.getState().position_;
    return computeTrajectoryCommand(actual_, model.getState().velocity_, command_);
  }
  
}
