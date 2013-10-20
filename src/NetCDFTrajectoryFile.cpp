#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <iostream>
#include <mpi.h>
#include "NetCDFTrajectoryFile.hpp"


NetCDFTrajectoryFile::NetCDFTrajectoryFile(const std::string& filename, const char* mode, int numAtoms=0): mode(mode), n_atoms(numAtoms) {

  NcFile::FileMode ncMode;
  if (strcmp(mode, "r") == 0) {
    handle = new NcFile(filename.c_str(), NcFile::ReadOnly);
    n_atoms = handle->get_dim("atom")->size();
  } else if (strcmp(mode, "w") == 0) {
    ncMode = NcFile::Replace;
    if (n_atoms <= 0) {
      printf("ERROR NUMBER OF ATOMS");
      exit(1);
    }
    handle = new NcFile(filename.c_str(), NcFile::Replace, NULL, 0, NcFile::Offset64Bits);
  } else {
    printf("ERROR BAD MODE");
    exit(1);
  }

  if (!handle->is_valid()) {
    printf("------------------\n");
    printf("ERROR OPENING FILE\n");
    printf("Mode=%s\n", mode);
    exit(1);
  } else{
    printf("FILE OPENEND SUCCESSFULLY\n");
  }
  

  if (strcmp(mode, "w") == 0) {
    initializeHeaders();
  }
}


int NetCDFTrajectoryFile::initializeHeaders() {
  handle->add_att("title", "");
  handle->add_att("application", "OpenMM");
  handle->add_att("program", "Tungsten");
  handle->add_att("programVersion", "0.1");
  handle->add_att("Conventions", "AMBER");
  handle->add_att("ConventionVersion", "1.0");

  NcDim* frameDim = handle->add_dim("frame", 0);
  NcDim* spatialDim = handle->add_dim("spatial", 3);
  NcDim* atomDim = handle->add_dim("atom", n_atoms);
  NcDim* cellSpatialDim = handle->add_dim("cell_spatial", 3);
  NcDim* cellAngularDim = handle->add_dim("cell_angular", 3);
  NcDim* labelDim = handle->add_dim("label", 5);

  NcVar* cellSpatialVar = handle->add_var("cell_spatial", ncChar, spatialDim);
  NcVar* cellAngularVar = handle->add_var("cell_angular", ncChar, spatialDim, labelDim);
  NcVar* cellLengthsVar = handle->add_var("cell_lengths", ncDouble, frameDim, cellSpatialDim);
  NcVar* cellAnglesVar = handle->add_var("cell_angles", ncDouble, frameDim, cellAngularDim);
  cellAnglesVar->add_att("units", "degree");

  cellSpatialVar->put("XYZ", 3);
  cellAngularVar->put("alpha", 1, 5);
  cellAngularVar->set_cur(1,0);
  cellAngularVar->put("beta", 1, 4);
  cellAngularVar->set_cur(2,0);
  cellAngularVar->put("gamma", 1, 5);

  NcVar* timeVar = handle->add_var("time", ncFloat, frameDim);
  timeVar->add_att("units", "picosecond");
  NcVar* coordVar = handle->add_var("coordinates", ncFloat, frameDim, atomDim, spatialDim);
  coordVar->add_att("units", "angstrom");
}
    

int NetCDFTrajectoryFile::write(OpenMM::State state) {
  if (strcmp(mode, "w") != 0)
    throw "Writing is not allowed in this mode";
  int frame = handle->get_dim("frame")->size();

  OpenMM::Vec3 a;
  OpenMM::Vec3 b;
  OpenMM::Vec3 c;
  double time = state.getTime();
  state.getPeriodicBoxVectors(a, b, c);
  double cellLengths[] = {a[0]*10.0, b[1]*10.0, c[2]*10.0};
  double cellAngles[] = {90.0, 90.0, 90.0};
  std::vector<OpenMM::Vec3> positions = state.getPositions();
  for (size_t i = 0; i < positions.size(); i++)
    positions[i] *= 10;

  handle->get_var("time")->put_rec(&time, frame);
  handle->get_var("cell_lengths")->put_rec(cellLengths, frame);
  handle->get_var("cell_angles")->put_rec(cellAngles, frame);
  handle->get_var("coordinates")->put_rec(&positions[0][0], frame);

  return 1;
}

void NetCDFTrajectoryFile::readPositions(int stride, float* out) const {
  int rank = MPI::COMM_WORLD.Get_rank();
  int numTotalFrames = handle->get_dim("frame")->size();
  int numFrames = (numTotalFrames+stride-1)/stride;
  int numAtoms = handle->get_dim("atom")->size();

  NcVar* coord = handle->get_var("coordinates");
  int ii = 0;
  for (int i = 0; i < numTotalFrames; i += stride) {
    coord->set_cur(i, 0, 0);
    coord->get(out+i*numAtoms*3, 1, numAtoms, 3);
    i++;
  }

  printf("RETURNING FROM READ POSITIONS: rank=%d\n", rank);
}
