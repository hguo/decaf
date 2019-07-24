#include "geometry_utils.h"
#include <math.h>
#include <algorithm> 
#include "misc.h"
#include "block.h"
#include <Eigen/Dense>

double triangle_signed_area(double *a, double *b, double *c, double radius){

  double ab = get_arc_length(a[0], a[1], a[2], b[0], b[1], b[2])/radius;
  double bc = get_arc_length(b[0], b[1], b[2], c[0], c[1], c[2])/radius;
  double ca = get_arc_length(c[0], c[1], c[2], a[0], a[1], a[2])/radius;
  double semiperim = 0.5 * (ab + bc + ca);


  double tanqe = sqrt(std::max(0.0,tan(0.5* semiperim) * tan(0.5* (semiperim - ab))
   * tan(0.5 * (semiperim - bc)) * tan(0.5 * (semiperim - ca))));


 double mpas_triangle_signed_area_sphere = 4.0 * radius * radius * atan(tanqe);

      // ! computing correct signs (in similar fashion to mpas_sphere_angle)

 double ablen[3], aclen[3], dlen[3];
 ablen[1] = b[1] - a[1];
 ablen[2] = b[2] - a[2];
 ablen[0] = b[0] - a[0];

 aclen[1] = c[1] - a[1];
 aclen[2] = c[2] - a[2];
 aclen[0] = c[0] - a[0];

 dlen[0] =   (ablen[1] * aclen[2]) - (ablen[2] * aclen[1]);
 dlen[1] = -((ablen[0] * aclen[2]) - (ablen[2] * aclen[0]));
 dlen[2] =   (ablen[0] * aclen[1]) - (ablen[1] * aclen[0]);

 if ((dlen[0]*a[0] + dlen[1]*a[1] + dlen[2]*a[2]) < 0.0) 
  mpas_triangle_signed_area_sphere = -mpas_triangle_signed_area_sphere;


return mpas_triangle_signed_area_sphere;
}

int get_vertical_id(int nLevels, double zLoc, double *zMid){

  for (int aLevel=0; aLevel<nLevels-1; aLevel++){

    if (zMid[aLevel+1]<=zLoc && zLoc <= zMid[aLevel]){
      // the point is bounded by the levels (store the bottom level)

      return aLevel;
    }


  }

  if (zLoc < zMid[nLevels-1]){
    // case where location is smallest value
    return -1;
  }else if (zLoc > zMid[0]){
    // case where location is largest value
    return 0;
  }

}


void get_nearby_cell_index(int nCells,
 const double *xc, 
 const double *yc, 
 const double *zc,
 const double xp, 
 const double yp, 
 const double zp,
 const block &mpas1, 
 int &lastCell, 
 const int *cellsOnCell,
 const int *nEdgesOnCell
 ){

  double pointRadius, xPoint[3];
  std::map<int, Eigen::Array3d> xCell;
  int aPoint; 
  int cellID;

  if (lastCell<0){
        // // brute force solution
        // Eigen::VectorXd q(3);
        // q<<xp, yp, zp;
        // // get nearest cell neighbor ids using mpas_c
        // Eigen::VectorXi nearest_cell_idx(1);
        // Eigen::VectorXd dists2_cell(1);
        // mpas1.nns_cells->knn(q, nearest_cell_idx,dists2_cell, 1,0, Nabo::NNSearchF::SORT_RESULTS| Nabo::NNSearchF::ALLOW_SELF_MATCH);
        // cellID = nearest_cell_idx[0] ; // cell local id

  }else{
    
    int cellGuess =lastCell;
    cellID = -1;

    while (cellID != cellGuess){

    // we have a known cell
    cellID = cellGuess;


    // normalize locations to same spherical shell (unit) for direct comparison

    pointRadius = sqrt(xp*xp + yp*yp + zp*zp);
    xPoint[0] = xp/pointRadius; xPoint[1] = yp/pointRadius; xPoint[2] = zp/pointRadius;

    // for point itself
    // pointRadius = sqrt(xc[cellID]*xc[cellID] + yc[cellID]*yc[cellID] + zc[cellID]*zc[cellID]);
    pointRadius = mpas1.cradius;

    // dprint("map size %ld", mpas1.cellIndex.size());

    // if (mpas1.gid==1){
    //   std::for_each(mpas1.cellIndex.begin(), mpas1.cellIndex.end(),
    //     [](std::pair<int, int> element){
    //       // Accessing KEY from element
    //       int key = element.first;
    //       // Accessing VALUE from element.
    //       int value = element.second;
    //       std::cout<<key<<" ";
    //     });
    // }


    int localCellID = mpas1.cellIndex.at(cellID+1);
    // xCell[cellID] << xc[localCellID],yc[localCellID],zc[localCellID]; xCell[cellID] /= pointRadius;

    // dprint("nEdgesOnCell[mpas1.cellIndex.at(cellID)] %d", nEdgesOnCell[mpas1.cellIndex.at(cellID)]);
    // exit(0);
      // for point neighbors
    int iPoint_=-7, aPoint_=-7;
      
      for (int iPoint=0; iPoint<nEdgesOnCell[localCellID]; iPoint++)
      {
          
         
          // aPoint = cellsOnCell[cellID*mpas1.maxEdges+iPoint] - 1; // local cell id of neighbor
          iPoint_ = iPoint;
          aPoint = cellsOnCell[localCellID*mpas1.maxEdges+iPoint] - 1; // local cell id of neighbor

          aPoint_ = aPoint;
          if (aPoint >= nCells || aPoint==-1) continue; // todo: nCells should be nCells_local
          // dprint("aPoint %d, %d, %d, nEdgesOnCell[cellID] %d", aPoint, cellID, iPoint, nEdgesOnCell[cellID]);

          try{
            int local_aPoint = mpas1.cellIndex.at(aPoint+1);
             }
      catch(...)
          {
          // catch any other errors (that we have no information about)
            dprint("EX aPoint_ %d, cellID %d, iPoint_ %d, nEdgesOnCell[cellID] %d, gid %d, gcIdxToGid %d", aPoint_, cellID, iPoint_, nEdgesOnCell[cellID], mpas1.gid, mpas1.gcIdxToGid[aPoint]);
            exit(0);
          }
           /*
            // xCell[aPoint] << xc[local_aPoint], yc[local_aPoint], zc[local_aPoint]; xCell[aPoint] /= pointRadius;
         
          */

    }
    
  

      // dprint("cellID %d cellGuess %d", cellID, cellGuess);

    /*
        double dx = xPoint[0] - xCell[cellID][0];
        double dy = xPoint[1] - xCell[cellID][1];
        double dz = xPoint[2] - xCell[cellID][2];
        double r2Min = dx*dx + dy*dy + dz*dz;
        double r2;


        for (int iPoint=0; iPoint<nEdgesOnCell[cellID]; ++iPoint){
          aPoint = cellsOnCell[cellID*mpas1.maxEdges+iPoint];

          // compute squared distances

          dx = xPoint[0] - xCell[aPoint][0];
          dy = xPoint[1] - xCell[aPoint][1];
          dz = xPoint[2] - xCell[aPoint][2];
          r2 = dx*dx + dy*dy + dz*dz;
          if ( r2 < r2Min){
            // we have a new closest point
            cellGuess = aPoint;
            r2Min = r2;

          }


        }
        */
      }
    }





          lastCell = cellID;

        }
