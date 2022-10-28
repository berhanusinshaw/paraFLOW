/*BHEADER*********************************************************************
 *
 *  Copyright (c) 1995-2009, Lawrence Livermore National Security,
 *  LLC. Produced at the Lawrence Livermore National Laboratory. Written
 *  by the Parflow Team (see the CONTRIBUTORS file)
 *  <parflow@lists.llnl.gov> CODE-OCEC-08-103. All rights reserved.
 *
 *  This file is part of Parflow. For details, see
 *  http://www.llnl.gov/casc/parflow
 *
 *  Please read the COPYRIGHT file or Our Notice and the LICENSE file
 *  for the GNU Lesser General Public License.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License (as published
 *  by the Free Software Foundation) version 2.1 dated February 1999.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms
 *  and conditions of the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 **********************************************************************EHEADER*/

#include "parflow.h"

/*--------------------------------------------------------------------------
 * Structures
 *--------------------------------------------------------------------------*/

typedef struct {
  int num_phases;

  int     *type;
  void   **data;
} PublicXtra;

typedef void InstanceXtra;

typedef struct {
  NameArray regions;
  int num_regions;
  int     *region_indices;
  double  *values;
} Type0;                       /* constant regions */


/*--------------------------------------------------------------------------
 * ICPhaseSatur
 *--------------------------------------------------------------------------*/

void         ICPhaseSatur(
                          Vector *     ic_phase_satur,
                          int          phase,
                          ProblemData *problem_data)
{
  PFModule      *this_module = ThisPFModule;
  PublicXtra    *public_xtra = (PublicXtra*)PFModulePublicXtra(this_module);

  Grid          *grid = VectorGrid(ic_phase_satur);

  Type0          *dummy0;

  SubgridArray   *subgrids = GridSubgrids(grid);

  Subgrid        *subgrid;
  Subvector      *ps_sub;

  double         *data;

  int ix, iy, iz;
  int nx, ny, nz;
  int r;

  double field_sum;

  int is, i, j, k, ips;


  /*-----------------------------------------------------------------------
   * Initial saturation conditions for this phase
   *-----------------------------------------------------------------------*/

  InitVector(ic_phase_satur, 0.0);

  switch ((public_xtra->type[phase]))
  {
    case 0:
    {
      int num_regions;
      int     *region_indices;
      double  *values;

      GrGeomSolid  *gr_solid;
      double value;
      int ir;


      dummy0 = (Type0*)(public_xtra->data[phase]);

      num_regions = (dummy0->num_regions);
      region_indices = (dummy0->region_indices);
      values = (dummy0->values);

      for (ir = 0; ir < num_regions; ir++)
      {
        gr_solid = ProblemDataGrSolid(problem_data, region_indices[ir]);
        value = values[ir];

        ForSubgridI(is, subgrids)
        {
          subgrid = SubgridArraySubgrid(subgrids, is);
          ps_sub = VectorSubvector(ic_phase_satur, is);

          ix = SubgridIX(subgrid);
          iy = SubgridIY(subgrid);
          iz = SubgridIZ(subgrid);

          nx = SubgridNX(subgrid);
          ny = SubgridNY(subgrid);
          nz = SubgridNZ(subgrid);

          /* RDF: assume resolution is the same in all 3 directions */
          r = SubgridRX(subgrid);

          data = SubvectorData(ps_sub);
          GrGeomInLoop(i, j, k, gr_solid, r, ix, iy, iz, nx, ny, nz,
          {
            ips = SubvectorEltIndex(ps_sub, i, j, k);

            data[ips] = value;
          });
        }
      }

      break;
    }
  }

#if 1
  {
    /*************************************************************************
    *             Informational computation and printing.                   *
    *************************************************************************/

    field_sum = ComputeTotalConcen(ProblemDataGrDomain(problem_data),
                                   grid, ic_phase_satur);

    if (!amps_Rank(amps_CommWorld))
    {
      amps_Printf("Initial saturation volume for phase %2d = %f\n", phase, field_sum);
    }
  }
#endif
}

/*--------------------------------------------------------------------------
 * ICPhaseSaturInitInstanceXtra
 *--------------------------------------------------------------------------*/

PFModule  *ICPhaseSaturInitInstanceXtra()
{
  PFModule      *this_module = ThisPFModule;
  InstanceXtra  *instance_xtra;


#if 0
  if (PFModuleInstanceXtra(this_module) == NULL)
    instance_xtra = ctalloc(InstanceXtra, 1);
  else
    instance_xtra = (InstanceXtra*)PFModuleInstanceXtra(this_module);
#endif
  instance_xtra = NULL;

  PFModuleInstanceXtra(this_module) = instance_xtra;

  return this_module;
}

/*-------------------------------------------------------------------------
 * ICPhaseSaturFreeInstanceXtra
 *-------------------------------------------------------------------------*/

void  ICPhaseSaturFreeInstanceXtra()
{
  PFModule      *this_module = ThisPFModule;
  InstanceXtra  *instance_xtra = (InstanceXtra*)PFModuleInstanceXtra(this_module);


  if (instance_xtra)
  {
    tfree(instance_xtra);
  }
}

/*--------------------------------------------------------------------------
 * ICPhaseSaturNewPublicXtra
 *--------------------------------------------------------------------------*/

PFModule   *ICPhaseSaturNewPublicXtra(
                                      int num_phases)
{
  PFModule      *this_module = ThisPFModule;
  PublicXtra    *public_xtra;

  Type0         *dummy0;

  int i;

  char *phase_name;
  char *switch_name;
  char *region;
  char key[IDB_MAX_KEY_LEN];

  NameArray type_na;

  type_na = NA_NewNameArray("Constant");

  public_xtra = ctalloc(PublicXtra, 1);

  (public_xtra->num_phases) = num_phases;

  (public_xtra->type) = ctalloc(int, num_phases);
  (public_xtra->data) = ctalloc(void *, num_phases);

  for (i = 0; i < num_phases - 1; i++)
  {
    phase_name = NA_IndexToName(GlobalsPhaseNames, i);

    sprintf(key, "ICSaturation.%s.Type", phase_name);
    switch_name = GetString(key);
    public_xtra->type[i] = NA_NameToIndexExitOnError(type_na, switch_name, key);

    switch ((public_xtra->type[i]))
    {
      case 0:
      {
        int num_regions, ir;

        dummy0 = ctalloc(Type0, 1);

        sprintf(key, "ICSaturation.%s.GeomNames", phase_name);
        switch_name = GetString(key);
        dummy0->regions = NA_NewNameArray(switch_name);

        dummy0->num_regions = NA_Sizeof(dummy0->regions);

        num_regions = (dummy0->num_regions);

        (dummy0->region_indices) = ctalloc(int, num_regions);
        (dummy0->values) = ctalloc(double, num_regions);

        for (ir = 0; ir < num_regions; ir++)
        {
          region = NA_IndexToName(dummy0->regions, ir);

          dummy0->region_indices[ir] =
            NA_NameToIndex(GlobalsGeomNames, region);

          sprintf(key, "Geom.%s.ICSaturation.%s.Value", region, phase_name);
          dummy0->values[ir] = GetDouble(key);
        }

        (public_xtra->data[i]) = (void*)dummy0;

        break;
      }

      default:
      {
            InputError("Invalid switch value <%s> for key <%s>", switch_name, key);
      }
    }
  }

  NA_FreeNameArray(type_na);

  PFModulePublicXtra(this_module) = public_xtra;
  return this_module;
}

/*--------------------------------------------------------------------------
 * ICPhaseSaturFreePublicXtra
 *--------------------------------------------------------------------------*/

void  ICPhaseSaturFreePublicXtra()
{
  PFModule    *this_module = ThisPFModule;
  PublicXtra  *public_xtra = (PublicXtra*)PFModulePublicXtra(this_module);

  Type0       *dummy0;

  int i;


  if (public_xtra)
  {
    for (i = 0; i < (public_xtra->num_phases) - 1; i++)
    {
      switch ((public_xtra->type[i]))
      {
        case 0:
        {
          dummy0 = (Type0*)(public_xtra->data[i]);

          NA_FreeNameArray(dummy0->regions);

          tfree(dummy0->region_indices);
          tfree(dummy0->values);
          tfree(dummy0);
          break;
        }
      }
    }

    tfree(public_xtra->data);
    tfree(public_xtra->type);

    tfree(public_xtra);
  }
}

/*--------------------------------------------------------------------------
 * ICPhaseSaturSizeOfTempData
 *--------------------------------------------------------------------------*/

int  ICPhaseSaturSizeOfTempData()
{
  return 0;
}
