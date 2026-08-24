/****************************************************************************
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "ovrtx_scene_sync.h"

#include "../base/document.h"
#include "../base/document_tree_node.h"
#include "../base/global.h"
#include "../base/libtree.h"
#include "../base/mesh_access.h"
#include "../graphics/graphics_object_ptr.h"
#include "../gui/gui_document.h"

#include <AIS_InteractiveObject.hxx>
#include <Graphic3d_Camera.hxx>
#include <Poly_Triangulation.hxx>
#include <Quantity_Color.hxx>
#include <TopLoc_Location.hxx>
#include <V3d_View.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>

namespace Mayo {
namespace Ovrtx {

namespace {

Vec3f toVec3(const gp_XYZ& p)
{
    return { float(p.X()), float(p.Y()), float(p.Z()) };
}

} // namespace

CameraState cameraFromV3dView(const OccHandle<V3d_View>& view, int width, int height)
{
    CameraState cam;
    cam.width = std::max(1, width);
    cam.height = std::max(1, height);
    cam.aspect = float(cam.width) / float(cam.height);
    if (view.IsNull() || view->Camera().IsNull())
        return cam;

    const auto& gc = view->Camera();
    cam.eye = zUpToYUp(toVec3(gc->Eye().XYZ()));
    cam.center = zUpToYUp(toVec3(gc->Center().XYZ()));
    cam.up = zUpToYUp(toVec3(gc->Up().XYZ()));
    cam.fovYDegrees = float(gc->FOVy());
    cam.orthographic = gc->IsOrthographic();
    cam.orthoHeight = float(gc->Scale());
    cam.zNear = float(std::max(gc->ZNear(), 1e-4));
    cam.zFar = float(std::max(gc->ZFar(), double(cam.zNear) * 10.));
    return cam;
}

UsdScene collectSceneFromGuiDocument(const GuiDocument* guiDoc, int width, int height)
{
    UsdScene scene;
    if (!guiDoc)
        return scene;

    scene.camera = cameraFromV3dView(guiDoc->v3dView(), width, height);
    const DocumentPtr& doc = guiDoc->document();
    if (!doc)
        return scene;

    int meshIndex = 0;
    traverseTree(doc->modelTree(), [&](TreeNodeId nodeId) {
        if (guiDoc->nodeVisibleState(nodeId) == CheckState::Off)
            return;

        gp_Trsf aisTrsf;
        guiDoc->foreachGraphicsObject(nodeId, [&](const GraphicsObjectPtr& obj) {
            if (!obj.IsNull())
                aisTrsf = obj->Transformation();
        });

        const DocumentTreeNode treeNode(doc, nodeId);
        IMeshAccess_visitMeshes(treeNode, [&](const IMeshAccess& access) {
            const OccHandle<Poly_Triangulation>& tri = access.triangulation();
            if (tri.IsNull() || tri->NbNodes() < 3 || tri->NbTriangles() < 1)
                return;

            const gp_Trsf trsf = aisTrsf * access.location().Transformation();
            UsdMesh mesh;
            mesh.primName = sanitizePrimName("Part", meshIndex++);
            mesh.points.reserve(static_cast<size_t>(tri->NbNodes()));
            mesh.faceVertexIndices.reserve(static_cast<size_t>(tri->NbTriangles() * 3));

            if (auto c = access.nodeColor(1)) {
                mesh.displayColor = { float(c->Red()), float(c->Green()), float(c->Blue()) };
            }
            else if (auto c0 = access.nodeColor(0)) {
                mesh.displayColor = { float(c0->Red()), float(c0->Green()), float(c0->Blue()) };
            }

            for (int i = 1; i <= tri->NbNodes(); ++i) {
                const gp_Pnt p = tri->Node(i).Transformed(trsf);
                mesh.points.push_back(zUpToYUp(toVec3(p.XYZ())));
            }

            if (tri->HasNormals()) {
                mesh.normals.reserve(mesh.points.size());
                for (int i = 1; i <= tri->NbNodes(); ++i) {
                    gp_Dir n = tri->Normal(i);
                    n.Transform(trsf);
                    mesh.normals.push_back(zUpToYUp(toVec3(n.XYZ())));
                }
            }

            for (int t = 1; t <= tri->NbTriangles(); ++t) {
                int n1 = 0, n2 = 0, n3 = 0;
                tri->Triangle(t).Get(n1, n2, n3);
                mesh.faceVertexIndices.push_back(n1 - 1);
                mesh.faceVertexIndices.push_back(n2 - 1);
                mesh.faceVertexIndices.push_back(n3 - 1);
            }

            scene.meshes.push_back(std::move(mesh));
        });
    });

    return scene;
}

} // namespace Ovrtx
} // namespace Mayo
