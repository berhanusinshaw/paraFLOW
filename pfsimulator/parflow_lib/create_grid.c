/*BHEADER**********************************************************************

  Copyright (c) 1995-2009, Lawrence Livermore National Security,
  LLC. Produced at the Lawrence Livermore National Laboratory. Written
  by the Parflow Team (see the CONTRIBUTORS file)
  <parflow@lists.llnl.gov> CODE-OCEC-08-103. All rights reserved.

  This file is part of Parflow. For details, see
  http://www.llnl.gov/casc/parflow

  Please read the COPYRIGHT file or Our Notice and the LICENSE file
  for the GNU Lesser General Public License.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License (as published
  by the Free Software Foundation) version 2.1 dated February 1999.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
  and conditions of the GNU General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
**********************************************************************EHEADER*/

/******************************************************************************
 *
 * Create the distributed grid from the user-grid input
 *
 *****************************************************************************/

#include "parflow.h"
#ifdef HAVE_P4EST
#include "../p4est_test/parflow_p4est.h"
#endif

/*--------------------------------------------------------------------------
 * GetGridSubgrids:
 *   Returns a SubgridArray containing subgrids on my process.
 *
 *   Notes:
 *     The ordering of `subgrids' is inferred by `all_subgrids'.
 *     The `subgrids' array points to subgrids in `all_subgrids'.
 *--------------------------------------------------------------------------*/

SubgridArray  *GetGridSubgrids(
   SubgridArray  *all_subgrids)
{
   SubgridArray  *subgrids;

   Subgrid       *s;

   int            i, my_proc;


   my_proc   = amps_Rank(amps_CommWorld);

   subgrids = NewSubgridArray();

   ForSubgridI(i, all_subgrids)
   {
      s = SubgridArraySubgrid(all_subgrids, i);
      if (SubgridProcess(s) == my_proc)
	 AppendSubgrid(s, subgrids);
   }

   return subgrids;
}


/*--------------------------------------------------------------------------
 * CreateGrid:
 *   We currently assume that the user's grid consists of 1 subgrid only.
 *--------------------------------------------------------------------------*/

Grid           *CreateGrid(
   Grid           *user_grid)
{
   Grid        *grid;

   SubgridArray  *subgrids;
   SubgridArray  *all_subgrids;

#ifdef HAVE_P4EST
   int                Nx, Ny, Nz;
   int                mx, my, mz;
   int                Px, Py, Pz;
   int                lx, ly, lz;
   int                px, py, pz;
   int                ix, iy, iz;
   double             v[3];
   Subgrid            *user_subgrid;
   parflow_p4est_qiter_t *qiter;
#endif

#ifndef HAVE_P4EST
   /*-----------------------------------------------------------------------
    * Create all_subgrids
    *-----------------------------------------------------------------------*/

   if (!(all_subgrids = DistributeUserGrid(user_grid)))
   {
      if (!amps_Rank(amps_CommWorld))
         amps_Printf("Incorrect process allocation input\n");
      exit(1);
   }

   /*-----------------------------------------------------------------------
    * Create subgrids
    *-----------------------------------------------------------------------*/

   subgrids = GetGridSubgrids(all_subgrids);

   /*-----------------------------------------------------------------------
    * Create the grid.
    *-----------------------------------------------------------------------*/

   grid = NewGrid(subgrids, all_subgrids);

   /*-----------------------------------------------------------------------
    * Create communication packages.
    *-----------------------------------------------------------------------*/

   CreateComputePkgs(grid);

   // SGS Debug
   globals -> grid3d = grid;

#else
   grid = talloc(Grid, 1);
   user_subgrid = GridSubgrid(user_grid, 0);

   Nx = SubgridNX(user_subgrid);
   Ny = SubgridNY(user_subgrid);
   Nz = SubgridNZ(user_subgrid);

   mx = GlobalsSubgridPointsX;
   my = GlobalsSubgridPointsY;
   mz = GlobalsSubgridPointsZ;

   /* Compute number of subgrids per coordinate direction. */
   Px = Nx / mx;
   Py = Ny / my;
   Pz = (Nz == 1) ? 1 : Nz / mz;

   lx = Nx % mx;
   ly = Ny % my;
   lz = Nz % mz;

   /* Create the pfgrid. */
   grid->pfgrid = parflow_p4est_grid_new (Px, Py, Pz);

   /* Loop on the quadrants (leafs) of this forest
      and attach a subgrid on each */
   for (qiter = parflow_p4est_qiter_init(grid->pfgrid);
        parflow_p4est_qiter_isvalid(qiter);
        parflow_p4est_qiter_next(qiter)) {

       /* Get bottom left corner (anchor node)  in
            * index space for the new subgrid */
       parflow_p4est_qiter_qcorner(qiter, v);

       /* Decide the dimensions for the new subgrid */
       ix = (int) v[0];
       iy = (int) v[1];
       iz = (int) v[2];

       px =  ix < lx ? mx + 1 : mx;
       py =  iy < ly ? my + 1 : my;
       if (Nz > 1){
           pz = iz < lz ? mz + 1 : mz;
         }
       else{
           pz = 1;
         }

       /* Allocate new subgrid and attach it to this quadrant */
       user_subgrid = NewSubgrid(ix, iy, iz, px, py, pz,
                                          0,  0,  0, 99999);
       parflow_p4est_qiter_set_data(qiter, (void*) user_subgrid);
    }
    parflow_p4est_qiter_destroy (qiter);

#if 0
   /* Loop over the ghost layer */
   G = (int) ghost_layer->elem_count;
   P4EST_ASSERT( Q >= 0 );
   for (q = 0; q < G; ++q) {
       quad = p4est_quadrant_array_index (ghost_layer, (size_t) q);
       level_factor = pow (2., quad->level);
       gt = parflow_p4est_gquad_owner_tree (quad);
       parflow_p4est_qcoord_to_vertex (grid->pfgrid, gt, quad, v);

       /* Get bottom left corner (anchor node)  in
        * index space for the new subgrid */
       ix = (int) level_factor * v[0];
       iy = (int) level_factor * v[1];
       iz = (int) level_factor * v[2];

       /* Decide the dimensions for the new subgrid */
       px =  ix < lx ? mx + 1 : mx;
       py =  iy < ly ? my + 1 : my;
       if (Nz > 1){
          pz = iz < lz ? mz + 1 : mz;
       }
       else{
          pz = 1;
       }

       /* Allocate new subgrid and attach it to this
        * ghost quadrant */
       quad->p.user_data =
           (void *) NewSubgrid(ix, iy, iz, px, py, pz,
                                0,  0,  0, -9999);
   }
#endif
#endif

   return grid;
}

