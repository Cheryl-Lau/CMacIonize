/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2016 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
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
 * @file testPhotonSource.cpp
 *
 * @brief Unit test for the PhotonSource class.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "CoordinateVector.hpp"
#include "Photon.hpp"
#include "PhotonSource.hpp"
#include "SingleStarPhotonSourceDistribution.hpp"
#include <cassert>
using namespace std;

/**
 * @brief Unit test for the PhotonSource class.
 *
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return Exit code: 0 on success.
 */
int main(int argc, char **argv) {
  SingleStarPhotonSourceDistribution distribution(
      CoordinateVector(0.5, 0.5, 0.5));

  PhotonSource source(distribution);

  // check if the returned position is what we expect it to be
  {
    Photon photon = source.get_random_photon();
    assert(photon.get_position().x() == 0.5);
    assert(photon.get_position().y() == 0.5);
    assert(photon.get_position().z() == 0.5);
  }

  return 0;
}
