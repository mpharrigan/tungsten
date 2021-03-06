#include "mpi.h"
#include "stdlib.h"
#include <vector>
#include <numeric>
#include "csparse.h"
#include "utilities.hpp"
#include "ParallelMSM.hpp"
#define MASTER 0

using std::vector;

/*
 * Note that numState must be the same accross all of the nodes
 *
 */
ParallelMSM::ParallelMSM(const std::vector<int>& assignments, int numStates):
  assignments(assignments),
  numStates(numStates) {
  static const int rank = MPI::COMM_WORLD.Get_rank();
  static const int size = MPI::COMM_WORLD.Get_size();

  // this will probably fail if assignments is empty
  cs* T = cs_spalloc(numStates, numStates, 1, 1, 1);
  // Add the pairs
  for (int i = 0; i < assignments.size()-1; i++)
    cs_entry(T, assignments[i], assignments[i+1], 1.0);

  // Convert to CSC form and sum duplicace entries
  counts = cs_triplet(T);
  cs_dupl(counts);
  cs_free(T);

  // Gather nzmax on root, the maximum number of entries
  // each each rank's counts
  vector<int> rootNzmax(size);
  MPI::COMM_WORLD.Gather(&counts->nzmax, 1, MPI_INT, &rootNzmax[0], 1, MPI_INT, MASTER);

  cs* newCounts;
  if (rank != MASTER) {
    // All of the slave nodes send their buffers to to MASTER
    // for accumulation
    MPI::COMM_WORLD.Isend(counts->p, counts->n+1, MPI_INT, MASTER, 0);
    MPI::COMM_WORLD.Isend(counts->i, counts->nzmax, MPI_INT, MASTER, 1);
    MPI::COMM_WORLD.Isend(counts->x, counts->nzmax, MPI_DOUBLE, MASTER, 2);
  } else {
    for (int j = 1; j < size; j++) {
      // The master node receives these entries and uses them to
      // reconstruct a sparse matrix, using cs_add to then
      // add it to its own.
      vector<int> p(numStates+1);
      vector<int> i(rootNzmax[j]);
      vector<double> x(rootNzmax[j]);
      MPI::Request rP, rI, rX;
      rP = MPI::COMM_WORLD.Irecv(&p[0], numStates+1, MPI_INT, j, 0);
      rI = MPI::COMM_WORLD.Irecv(&i[0], rootNzmax[j], MPI_INT, j, 1);
      rX = MPI::COMM_WORLD.Irecv(&x[0], rootNzmax[j], MPI_DOUBLE, j, 2);
      rP.Wait();
      rI.Wait();
      rX.Wait();

      // place this data in a struct
      cs M;
      M.nzmax = rootNzmax[j];
      M.m = numStates;
      M.n = numStates;
      M.p = &p[0];
      M.i = &i[0];
      M.x = &x[0];
      M.nz = -1;

      newCounts = cs_add(counts, &M, 1.0, 1.0);
      cs_free(counts);
      counts = newCounts;
    }
  }

  if (rank == MASTER) {
    printf("\nFinal Data\n");
    cs_print(counts, 0);
  }

}

ParallelMSM::~ParallelMSM() {
  cs_free(counts);
}
