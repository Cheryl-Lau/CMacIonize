/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2018 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
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
 * @file RescaledICHydroMask.hpp
 *
 * @brief Masked out region where the hydrodynamics is artificially reset to a
 * constant value.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef RESCALEDICHYDROMASK_HPP
#define RESCALEDICHYDROMASK_HPP

#include "BlockSyntaxDensityFunction.hpp"
#include "HydroMask.hpp"
#include "Utilities.hpp"

#include <cinttypes>
#include <map>

/**
 * @brief Masked out region where the hydrodynamics is artificially reset to a
 * constant value.
 */
class RescaledICHydroMask : public HydroMask {
private:
  /*! @brief Position of the center of the mask (in m). */
  const CoordinateVector<> _center;

  /*! @brief Radius squared of the mask (in m^2). */
  const double _radius2;

  /*! @brief Scale factors for the hydrodynamic variables. */
  const double _scale_factors[3];

  /*! @brief Mass accretion output interval (in s). */
  const double _delta_t;

  /*! @brief Mass accretion output counter. */
  uint_fast32_t _snap_n;

  /*! @brief Mask density (in kg m^-3). */
  double _mask_density;

  /*! @brief Reference mask velocity (in m s^-1). */
  double _mask_velocity;

  /*! @brief Mask pressure (in kg m^-1 s^-2). */
  double _mask_pressure;

  /*! @brief Mask velocities for all cells within the mask (in m s^-1). */
  std::vector< CoordinateVector<> > _mask_velocities;

  /*! @brief Map that connects subgrid indices to offsets in the mask
   *  velocities array. */
  std::map< uint_fast32_t, uint_fast32_t > _subgrid_offsets;

  /**
   * @brief Check if the given position is inside the mask region.
   *
   * @param position Position (in m).
   * @return True if the position is inside the mask region.
   */
  inline bool is_inside(const CoordinateVector<> position) const {
    return (position - _center).norm2() < _radius2;
  }

public:
  /**
   * @brief Constructor.
   *
   * @param center Position of the center of the mask (in m).
   * @param radius Radius of the mask (in m).
   * @param scale_factor_density Scale factor for the density.
   * @param scale_factor_velocity Scale factor for the velocity.
   * @param scale_factor_pressure Scale factor for the pressure.
   * @param delta_t Mass accretion output interval (in s).
   */
  inline RescaledICHydroMask(const CoordinateVector<> center,
                             const double radius,
                             const double scale_factor_density,
                             const double scale_factor_velocity,
                             const double scale_factor_pressure,
                             const double delta_t = 0.)
      : _center(center),
        _radius2(radius * radius), _scale_factors{scale_factor_density,
                                                  scale_factor_velocity,
                                                  scale_factor_pressure},
        _delta_t(delta_t), _snap_n(0), _mask_density(DBL_MAX),
        _mask_velocity(DBL_MAX), _mask_pressure(DBL_MAX) {}

  /**
   * @brief ParameterFile constructor.
   *
   * Parameters are:
   *  - center: Position of the center of the mask (default: [0. m, 0. m, 0. m])
   *  - radius: Radius of the mask (default: 1. m)
   *  - scale factor density: Scale factor for the density (default: 0.01)
   *  - scale factor velocity: Scale factor for the velocity (default: 1.)
   *  - scale factor pressure: Scale factors for the pressure (default: 0.01)
   *  - delta t: Mass accretion output interval (default: 5000. yr)
   *
   * @param params ParameterFile to read.
   */
  inline RescaledICHydroMask(ParameterFile &params)
      : RescaledICHydroMask(
            params.get_physical_vector< QUANTITY_LENGTH >("HydroMask:center",
                                                          "[0. m, 0. m, 0. m]"),
            params.get_physical_value< QUANTITY_LENGTH >("HydroMask:radius",
                                                         "1. m"),
            params.get_value< double >("HydroMask:scale factor density", 0.01),
            params.get_value< double >("HydroMask:scale factor velocity", 1.),
            params.get_value< double >("HydroMask:scale factor pressure", 0.01),
            params.get_physical_value< QUANTITY_TIME >("HydroMask:delta t",
                                                       "5000. yr")) {}

  /**
   * @brief Virtual destructor.
   */
  virtual ~RescaledICHydroMask() {}

  /**
   * @brief Initialize the mask values based on the given grid.
   *
   * @param grid DensityGrid to read from.
   */
  virtual void initialize_mask(DensityGrid &grid) {

    double min_rho = _mask_density;
    double min_v = _mask_velocity;
    double min_P = _mask_pressure;
    for (auto it = grid.begin(); it != grid.end(); ++it) {
      if (is_inside(it.get_cell_midpoint())) {
        const HydroVariables &hydro_variables = it.get_hydro_variables();
        const double rho = hydro_variables.get_primitives_density();
        const double v = hydro_variables.get_primitives_velocity().norm();
        const double P = hydro_variables.get_primitives_pressure();

        min_rho = std::min(min_rho, rho);
        min_v = std::min(min_v, v);
        min_P = std::min(min_P, P);
      }
    }

    min_rho *= _scale_factors[0];
    min_v *= _scale_factors[1];
    min_P *= _scale_factors[2];

    _mask_density = min_rho;
    _mask_pressure = min_P;

    for (auto it = grid.begin(); it != grid.end(); ++it) {
      if (is_inside(it.get_cell_midpoint())) {
        CoordinateVector<> v =
            it.get_hydro_variables().get_primitives_velocity();
        const double factor = min_v / v.norm();
        v *= factor;
        _mask_velocities.push_back(v);
      }
    }
  }

  /**
   * @brief Initialize the mask before the first hydrodynamical time step.
   *
   * Note that this method is not thread safe and should only be called in
   * serial!
   *
   * @param index Index of this subgrid in the subgrid list.
   * @param subgrid HydroDensitySubGrid to read from.
   */
  virtual void initialize_mask(const uint_fast32_t index,
                               HydroDensitySubGrid &subgrid) {

    _subgrid_offsets[index] = _mask_velocities.size();
    double min_rho = _mask_density;
    double min_v = _mask_velocity;
    double min_P = _mask_pressure;
    for (auto it = subgrid.hydro_begin(); it != subgrid.hydro_end(); ++it) {
      if (is_inside(it.get_cell_midpoint())) {
        const HydroVariables &hydro_variables = it.get_hydro_variables();
        const double rho = hydro_variables.get_primitives_density();
        const double v = hydro_variables.get_primitives_velocity().norm();
        const double P = hydro_variables.get_primitives_pressure();

        min_rho = std::min(min_rho, rho);
        min_v = std::min(min_v, v);
        min_P = std::min(min_P, P);

        _mask_velocities.push_back(v);
      }
    }

    _mask_density = min_rho;
    _mask_velocity = min_v;
    _mask_pressure = min_P;
  }

  /**
   * @brief Apply the mask to the given DensityGrid.
   *
   * We assume the cell order has not changed since intialize_mask() was called.
   *
   * @param grid DensityGrid to update.
   * @param actual_timestep Current system time step (in s).
   * @param current_time Current system time (in s).
   */
  virtual void apply_mask(DensityGrid &grid, const double actual_timestep,
                          const double current_time) {

    std::ofstream *mass_file = nullptr;
    if (_delta_t > 0 && current_time >= _snap_n * _delta_t) {
      std::string filename =
          Utilities::compose_filename("", "mass_accretion_", "txt", _snap_n, 3);
      mass_file = new std::ofstream(filename);
      *mass_file << "#time (s)\tmass difference (kg)\ttimestep (s)\txpos (m)\t"
                    "ypos (m)\tzpos (m)\n";
      ++_snap_n;
    }

    uint_fast32_t index = 0;
    for (auto it = grid.begin(); it != grid.end(); ++it) {

      if (is_inside(it.get_cell_midpoint())) {

        const double old_density =
            it.get_hydro_variables().get_primitives_density();
        const CoordinateVector<> old_velocity =
            it.get_hydro_variables().get_primitives_velocity();
        const double old_pressure =
            it.get_hydro_variables().get_primitives_pressure();
        const double old_energy =
            it.get_hydro_variables().get_conserved_total_energy();
        const double A =
            (old_energy - 0.5 * old_density * old_velocity.norm2()) /
            old_pressure;

        const double density = _mask_density;
        const CoordinateVector<> velocity = _mask_velocities[index];
        const double pressure = _mask_pressure;

        // set the primitive variables
        it.get_hydro_variables().set_primitives_density(density);
        it.get_hydro_variables().set_primitives_velocity(velocity);
        it.get_hydro_variables().set_primitives_pressure(pressure);

        const double volume = it.get_volume();
        const double mass = density * volume;
        const CoordinateVector<> momentum = mass * velocity;
        const double ekin = CoordinateVector<>::dot_product(velocity, momentum);
        // E = V*(rho*u + 0.5*rho*v^2) = V*(P/(gamma-1) + 0.5*rho*v^2)
        const double total_energy = A * pressure + 0.5 * ekin;

        // print mass accretion in file
        if (mass_file != nullptr) {

          // get mass for the current cell in the mask
          const double mass_inflow =
              it.get_hydro_variables().get_conserved_mass();

          // compute the mass accretion
          const double mass_difference = mass_inflow - mass;

          // coordinates for accretion
          const double xcoord = it.get_cell_midpoint().x();
          const double ycoord = it.get_cell_midpoint().y();
          const double zcoord = it.get_cell_midpoint().z();

          *mass_file << current_time << "\t" << mass_difference << "\t"
                     << actual_timestep << "\t" << xcoord << "\t" << ycoord
                     << "\t" << zcoord << "\n";
        }

        // set conserved variables
        it.get_hydro_variables().set_conserved_mass(mass);
        it.get_hydro_variables().set_conserved_momentum(momentum);
        it.get_hydro_variables().set_conserved_total_energy(total_energy);

        // set neutral fractions to 0
        it.get_ionization_variables().set_ionic_fraction(ION_H_n, 0.);
#ifdef HAS_HELIUM
        it.get_ionization_variables().set_ionic_fraction(ION_He_n, 0.);
#endif

        ++index;
      }
    }
    if (mass_file != nullptr) {
      delete mass_file;
    }
  }

  /**
   * @brief Apply the mask to the given HydroDensitySubGrid.
   *
   * The primitive and conserved variables of all cells within the mask will be
   * updated, all other cells are left untouched.
   *
   * @param index Index of this subgrid in the subgrid list.
   * @param subgrid HydroDensitySubGrid to update.
   * @param actual_timestep Current system time step (in s).
   * @param current_time Current simulation time (in s).
   */
  virtual void apply_mask(const uint_fast32_t index,
                          HydroDensitySubGrid &subgrid,
                          const double actual_timestep,
                          const double current_time) {

    uint_fast32_t cell_index = _subgrid_offsets[index];
    for (auto it = subgrid.hydro_begin(); it != subgrid.hydro_end(); ++it) {

      if (is_inside(it.get_cell_midpoint())) {

        const double old_density =
            it.get_hydro_variables().get_primitives_density();
        const CoordinateVector<> old_velocity =
            it.get_hydro_variables().get_primitives_velocity();
        const double old_pressure =
            it.get_hydro_variables().get_primitives_pressure();
        const double old_energy =
            it.get_hydro_variables().get_conserved_total_energy();
        const double A =
            (old_energy - 0.5 * old_density * old_velocity.norm2()) /
            old_pressure;

        const double density = _mask_density * _scale_factors[0];
        CoordinateVector<> velocity;
        const double vnrm2 = _mask_velocities[cell_index].norm2();
        if (vnrm2 > 0.) {
          velocity = _mask_velocities[cell_index] *
                     (_mask_velocity * _scale_factors[1] / std::sqrt(vnrm2));
        }
        const double pressure = _mask_pressure * _scale_factors[2];

        // set the primitive variables
        it.get_hydro_variables().set_primitives_density(density);
        it.get_hydro_variables().set_primitives_velocity(velocity);
        it.get_hydro_variables().set_primitives_pressure(pressure);

        const double volume = it.get_volume();
        const double mass = density * volume;
        const CoordinateVector<> momentum = mass * velocity;
        const double ekin = CoordinateVector<>::dot_product(velocity, momentum);
        // E = V*(rho*u + 0.5*rho*v^2) = V*(P/(gamma-1) + 0.5*rho*v^2)
        const double total_energy = A * pressure + 0.5 * ekin;

        // set conserved variables
        it.get_hydro_variables().set_conserved_mass(mass);
        it.get_hydro_variables().set_conserved_momentum(momentum);
        it.get_hydro_variables().set_conserved_total_energy(total_energy);

        // set neutral fractions to 0
        it.get_ionization_variables().set_ionic_fraction(ION_H_n, 0.);
#ifdef HAS_HELIUM
        it.get_ionization_variables().set_ionic_fraction(ION_He_n, 0.);
#endif

        ++cell_index;
      }
    }
  }

  /**
   * @brief Write the mask to the given restart file.
   *
   * @param restart_writer RestartWriter to use.
   */
  virtual void write_restart_file(RestartWriter &restart_writer) const {

    _center.write_restart_file(restart_writer);
    restart_writer.write(_radius2);
    restart_writer.write(_scale_factors[0]);
    restart_writer.write(_scale_factors[1]);
    restart_writer.write(_scale_factors[2]);
    restart_writer.write(_delta_t);
    restart_writer.write(_snap_n);
    restart_writer.write(_mask_density);
    restart_writer.write(_mask_pressure);
    {
      const auto size = _mask_velocities.size();
      restart_writer.write(size);
      for (std::vector< CoordinateVector<> >::size_type i = 0; i < size; ++i) {
        _mask_velocities[i].write_restart_file(restart_writer);
      }
    }
    {
      const auto size = _subgrid_offsets.size();
      restart_writer.write(size);
      for (auto it = _subgrid_offsets.begin(); it != _subgrid_offsets.end();
           ++it) {
        restart_writer.write(it->first);
        restart_writer.write(it->second);
      }
    }
  }

  /**
   * @brief Restart constructor.
   *
   * @param restart_reader Restart file to read from.
   */
  inline RescaledICHydroMask(RestartReader &restart_reader)
      : _center(restart_reader), _radius2(restart_reader.read< double >()),
        _scale_factors{restart_reader.read< double >(),
                       restart_reader.read< double >(),
                       restart_reader.read< double >()},
        _delta_t(restart_reader.read< double >()),
        _snap_n(restart_reader.read< double >()),
        _mask_density(restart_reader.read< double >()),
        _mask_pressure(restart_reader.read< double >()) {

    const std::vector< CoordinateVector<> >::size_type size =
        restart_reader.read< std::vector< CoordinateVector<> >::size_type >();
    _mask_velocities.resize(size);
    for (std::vector< CoordinateVector<> >::size_type i = 0; i < size; ++i) {
      _mask_velocities[i] = CoordinateVector<>(restart_reader);
    }
    const std::map< uint_fast32_t, uint_fast32_t >::size_type msize =
        restart_reader
            .read< std::map< uint_fast32_t, uint_fast32_t >::size_type >();
    for (std::map< uint_fast32_t, uint_fast32_t >::size_type i = 0; i < msize;
         ++i) {
      const uint_fast32_t key = restart_reader.read< uint_fast32_t >();
      const uint_fast32_t val = restart_reader.read< uint_fast32_t >();
      _subgrid_offsets[key] = val;
    }
  }
};

#endif // RESCALEDICHYDROMASK_HPP
