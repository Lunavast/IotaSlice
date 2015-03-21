//
//  IotaSlice.h
//  IotaSlice
//
//  Created by Matthias Melcher on 01.03.13.
//  Copyright (c) 2013 Matthias Melcher. All rights reserved.
//

#ifndef __IotaSlice__IotaSlice__
#define __IotaSlice__IotaSlice__


#include <vector>

class ISVertex; 
class ISEdge;
class ISFace;
class ISMesh;


class ISVec3
{
public:
  ISVec3();
  ISVec3(const ISVec3&);
  ISVec3(double*);
  ISVec3(float, float, float);
  ISVec3& operator-=(const ISVec3&);
  ISVec3& operator+=(const ISVec3&);
  ISVec3& operator*=(double);
  double *dataPointer() { return pV; }
  ISVec3& cross(const ISVec3&);
  double normalize();
  void zero();
  void write(double*);
  void read(float*);
  void read(double*);
  double x() { return pV[0]; }
  double y() { return pV[1]; }
  double z() { return pV[2]; }
  void set(float, float, float);
  double pV[3];
};


class ISVertex
{
public:
  ISVertex();
  ISVertex(const ISVertex*);
  bool validNormal() { return pNNormal==1; }
  void addNormal(const ISVec3&);
  void averageNormal();
  void print();
  ISVec3 pPosition;
  ISVec3 pNormal;
  int pNNormal;
};


typedef ISVertex *ISVertexPtr;
typedef std::vector<ISVertex*> ISVertexList;

class ISEdge
{
public:
  ISEdge();
  ISVertex *findZ(double);
  ISVertex *vertex(int i, ISFace *f);
  ISFace *otherFace(ISFace *);
  ISVertex *otherVertex(ISVertex*);
  int indexIn(ISFace *);
  int nFaces();
  ISFace *pFace[2];
  ISVertex *pVertex[2];
};

typedef std::vector<ISEdge*> ISEdgeList;

class ISFace
{
public:
  ISFace();
  bool validNormal() { return pNNormal==1; }
  void rotateVertices();
  void print();
  int pointsBelowZ(double z);
  ISVertex *pVertex[3];
  ISEdge *pEdge[3];
  ISVec3 pNormal;
  int pNNormal;
  bool pUsed;
};

typedef ISFace *ISFacePtr;
typedef std::vector<ISFace*> ISFaceList;

class ISMesh
{
public:
  ISMesh();
  virtual ~ISMesh() { clear(); }
  virtual void clear();
  bool validate();
  void drawGouraud();
  void drawFlat(unsigned int);
  void drawShrunk(unsigned int, double);
  void drawEdges();
  void addFace(ISFace*);
  void clearFaceNormals();
  void clearVertexNormals();
  void clearNormals() { clearFaceNormals(); clearVertexNormals(); }
  void calculateFaceNormals();
  void calculateVertexNormals();
  void calculateNormals() { calculateFaceNormals(); calculateVertexNormals(); }
  void fixHoles();
  void fixHole(ISEdge*);
  ISEdge *findEdge(ISVertex*, ISVertex*);
  ISEdge *addEdge(ISVertex*, ISVertex*, ISFace*);
  ISVertexList vertexList;
  ISEdgeList edgeList;
  ISFaceList faceList;
};

typedef std::vector<ISMesh*> ISMeshList;

class ISMeshSlice : public ISMesh
{
public:
  ISMeshSlice();
  virtual ~ISMeshSlice();
  virtual void clear();
  void drawLidEdge();
  void tesselate();
  void addZSlice(const ISMesh&, double);
  void addFirstLidVertex(ISFace *isFace, double zMin);
  void addNextLidVertex(ISFacePtr &isFace, ISVertexPtr &vCutA, int &edgeIndex, double zMin);
  ISEdgeList lidEdgeList;
};


#endif /* defined(__IotaSlice__IotaSlice__) */
