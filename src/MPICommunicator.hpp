/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2017 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * CMacIonize is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CMacIonize is distributed in the hope that it will be useful,
 * but WITOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with CMacIonize. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/**
 * @file MPICommunicator.hpp
 *
 * @brief C++ wrapper around basic MPI functions.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef MPICOMMUNICATOR_HPP
#define MPICOMMUNICATOR_HPP

#include "Error.hpp"

#include <mpi.h>
#include <vector>

/**
 * @brief Aliases for MPI_Op types.
 */
enum MPIOperatorType {
  /*! @brief Take the sum of a variable across all processes. */
  MPI_SUM_OF_ALL_PROCESSES = 0
};

/**
 * @brief C++ wrapper around basic MPI functions.
 */
class MPICommunicator {
private:
  /*! @brief Rank of the local MPI process. */
  int _rank;

  /*! @brief Total number of MPI processes. */
  int _size;

public:
  /**
   * @brief Constructor.
   *
   * Calls MPI_Init(), sets up custom error handling, and initializes rank and
   * size variables.
   *
   * @param argc Number of command line arguments passed on to the main program.
   * @param argv Command line arguments passed on to the main program.
   */
  MPICommunicator(int &argc, char **argv) {
    int status = MPI_Init(&argc, &argv);
    if (status != MPI_SUCCESS) {
      cmac_error("Failed to initialize MPI!");
    }

    // make sure errors are handled by us, not by the MPI library
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    // get the size and rank
    status = MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
    if (status != MPI_SUCCESS) {
      cmac_error("Failed to obtain rank of MPI process!");
    }
    status = MPI_Comm_size(MPI_COMM_WORLD, &_size);
    if (status != MPI_SUCCESS) {
      cmac_error("Failed to obtain number of MPI processes!");
    }
  }

  /**
   * @brief Destructor.
   *
   * Calls MPI_Finalize().
   */
  ~MPICommunicator() {
    int status = MPI_Finalize();
    if (status != MPI_SUCCESS) {
      cmac_error("Failed to clean up MPI!");
    }
  }

  /**
   * @brief Get the rank of the local MPI process.
   *
   * @return Rank of the local MPI process.
   */
  int get_rank() { return _rank; }

  /**
   * @brief Get the total number of MPI processes.
   *
   * @return Total number of MPI processes.
   */
  int get_size() { return _size; }

  /**
   * @brief Template function that returns the MPI_Datatype corresponding to the
   * given template data type.
   *
   * This function needs to be specialized for every data type.
   *
   * @return MPI_Datatype corresponding to the given template data type.
   */
  template < typename _datatype_ > static MPI_Datatype get_datatype();

  /**
   * @brief Function that returns the MPI_Op corresponding to the given
   * MPIOperatorType.
   *
   * @param type MPIOperatorType.
   * @return MPI_Op corresponding to the given MPIOperatorType.
   */
  inline static MPI_Op get_operator(MPIOperatorType type) {
    switch (type) {
    case MPI_SUM_OF_ALL_PROCESSES:
      return MPI_SUM;
    default:
      cmac_error("Unknown MPIOperatorType: %i!", type);
      return 0;
    }
  }

  /**
   * @brief Reduce the elements of the given std::vector of objects, using the
   * given getter member function to obtain an object data member to reduce, and
   * the given setter member function to set the result of the reduction.
   *
   * @param v std::vector containing objects of a given template class type.
   * @param getter Member function of the given template class type that returns
   * a value of the given template data type that will be reduced across all
   * processes.
   * @param setter Member function of the given template class type that takes
   * a value of the given template data type and stores it in the class type
   * object after the reduction.
   */
  template < MPIOperatorType _operatortype_, typename _datatype_,
             typename _classtype_ >
  void reduce(std::vector< _classtype_ * > &v,
              _datatype_ (_classtype_::*getter)(),
              void (_classtype_::*setter)(_datatype_)) {
    // in place reduction does not work
    std::vector< _datatype_ > sendbuffer(v.size());
    std::vector< _datatype_ > recvbuffer(v.size());
    for (unsigned int i = 0; i < v.size(); ++i) {
      sendbuffer[i] = (v[i]->*(getter))();
    }
    MPI_Datatype dtype = get_datatype< _datatype_ >();
    MPI_Op otype = get_operator(_operatortype_);
    MPI_Allreduce(&sendbuffer[0], &recvbuffer[0], sendbuffer.size(), dtype,
                  otype, MPI_COMM_WORLD);
    for (unsigned int i = 0; i < v.size(); ++i) {
      (v[i]->*(setter))(recvbuffer[i]);
    }
  }
};

/**
 * @brief Template function that returns the MPI_Datatype corresponding to the
 * given template data type.
 *
 * Specialization for a double precision floating point value.
 *
 * @return MPI_DOUBLE.
 */
template <> MPI_Datatype MPICommunicator::get_datatype< double >() {
  return MPI_DOUBLE;
}

#endif // MPICOMMUNICATOR_HPP
