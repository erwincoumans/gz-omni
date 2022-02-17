/*
 * Copyright (C) 2022 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "IgnitionVisual.hpp"
#include "SceneImpl.hpp"

#include <ignition/common/Console.hh>
#include <ignition/math/Pose3.hh>

#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/xform.h>

#include <algorithm>
#include <chrono>  // std::chrono::seconds
#include <string>
#include <thread>  // std::this_thread::sleep_for
#include <vector>

using namespace std::chrono_literals;

namespace ignition
{
namespace omniverse
{
//////////////////////////////////////////////////
SceneImpl::SceneImpl(const std::string &_worldName, pxr::UsdStageRefPtr _stage)
{
  this->worldName = _worldName;
  this->stage = _stage;
}

//////////////////////////////////////////////////
SceneImpl::SharedPtr SceneImpl::Make(const std::string &_worldName,
                                     pxr::UsdStageRefPtr _stage)
{
  auto sp = std::make_shared<SceneImpl>(_worldName, _stage);
  sp->Init();
  return sp;
}

// //////////////////////////////////////////////////
// pxr::UsdStageRefPtr SceneImpl::Stage() const
// {
//   return this->stage;
// }

//////////////////////////////////////////////////
pxr::UsdPrim SceneImpl::GetPrimAtPath(const std::string &_path)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  auto prim = this->stage->GetPrimAtPath(pxr::SdfPath(_path));
  return prim;
}

//////////////////////////////////////////////////
void SceneImpl::SaveStage()
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  this->stage->Save();
}

//////////////////////////////////////////////////
pxr::UsdGeomCapsule SceneImpl::CreateCapsule(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomCapsule::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdGeomSphere SceneImpl::CreateSphere(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomSphere::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdGeomCube SceneImpl::CreateCube(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomCube::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdGeomSphere SceneImpl::CreateEllipsoid(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomSphere::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdGeomCylinder SceneImpl::CreateCylinder(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomCylinder::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdShadeMaterial SceneImpl::CreateMaterial(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdShadeMaterial::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdShadeShader SceneImpl::CreateShader(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdShadeShader::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdGeomXform SceneImpl::CreateXform(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomXform::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdGeomMesh SceneImpl::CreateMesh(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  return pxr::UsdGeomMesh::Define(this->stage, pxr::SdfPath(_name));
}

//////////////////////////////////////////////////
pxr::UsdPrim SceneImpl::CreateFixedJoint(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  pxr::TfToken usdPrimTypeName("PhysicsFixedJoint");
  return this->stage->DefinePrim(
    pxr::SdfPath(_name),
    usdPrimTypeName);
}

//////////////////////////////////////////////////
pxr::UsdPrim SceneImpl::CreateRevoluteJoint(const std::string &_name)
{
  std::unique_lock<std::mutex> lkStage(mutexStage);
  pxr::TfToken usdPrimTypeName("PhysicsRevoluteJoint");
  return this->stage->DefinePrim(
    pxr::SdfPath(_name),
    usdPrimTypeName);
}

//////////////////////////////////////////////////
bool SceneImpl::Init()
{
  modelThread = std::make_shared<std::thread>(&SceneImpl::modelWorker, this);

  std::vector<std::string> topics;
  node.TopicList(topics);

  for (auto const &topic : topics)
  {
    if (topic.find("/joint_state") != std::string::npos)
    {
      if (!node.Subscribe(topic, &SceneImpl::CallbackJoint, this))
      {
        ignerr << "Error subscribing to topic [" << topic << "]" << std::endl;
        return false;
      }
      else
      {
        ignmsg << "Subscribed to topic: [" << topic << "]" << std::endl;
      }
    }
  }

  std::string topic = "/world/" + worldName + "/pose/info";
  // Subscribe to a topic by registering a callback.
  if (!node.Subscribe(topic, &SceneImpl::CallbackPoses, this))
  {
    ignerr << "Error subscribing to topic [" << topic << "]" << std::endl;
    return false;
  }

  return true;
}

//////////////////////////////////////////////////
std::unordered_map<std::string, IgnitionModel> SceneImpl::GetModels()
{
  std::unordered_map<std::string, IgnitionModel> result;
  std::unique_lock<std::mutex> lkPose(poseMutex);
  result = this->models;
  return result;
}

void SceneImpl::modelWorker()
{
  auto printvector = [](const auto &v)
  {
    igndbg << "{ ";
    for (auto i : v) igndbg << i << ' ';
    igndbg << "} " << std::endl;
  };

  while (true)
  {
    std::set<std::string> modelNames;

    auto modelsTmp = GetModels();

    for (auto &model : modelsTmp)
    {
      modelNames.insert(model.first);
    }

    // Prepare the input parameters.
    ignition::msgs::Empty req;
    ignition::msgs::Scene rep;
    bool result;
    unsigned int timeout = 5000;
    bool executed = node.Request("/world/" + worldName + "/scene/info", req,
                                 timeout, rep, result);
    if (executed)
    {
      std::set<std::string> modelNamesReceived;

      for (auto &model : rep.model())
      {
        std::string modelName = model.name();
        std::replace(modelName.begin(), modelName.end(), ' ', '_');
        modelNamesReceived.insert(modelName);
      }

      std::vector<std::string> removed;
      std::vector<std::string> added;

      std::set_difference(modelNamesReceived.begin(), modelNamesReceived.end(),
                          modelNames.begin(), modelNames.end(),
                          std::inserter(added, added.begin()));

      std::set_difference(modelNames.begin(), modelNames.end(),
                          modelNamesReceived.begin(), modelNamesReceived.end(),
                          std::inserter(removed, removed.begin()));

      if (added.size() > 0)
      {
        igndbg << "added " << '\n';
        printvector(added);
      }

      if (removed.size() > 0 && modelNames.size() > 0)
      {
        igndbg << "removed " << '\n';
        printvector(removed);
        for (auto &removeModelName : removed)
        {
          auto it = modelsTmp.find(removeModelName);
          if (it != modelsTmp.end())
          {
            {
              std::unique_lock<std::mutex> lkStage(mutexStage);
              this->stage->RemovePrim(
                  pxr::SdfPath("/" + worldName + "/" + removeModelName));
            }
            {
              igndbg << "removed: " << removeModelName << '\n';
              std::unique_lock<std::mutex> lkPose(poseMutex);
              this->models.erase(removeModelName);
            }
          }
        }
      }

      for (auto &model : rep.model())
      {
        std::string modelName = model.name();
        std::replace(modelName.begin(), modelName.end(), ' ', '_');
        auto it = modelsTmp.find(modelName);
        if (it == modelsTmp.end())
        {
          IgnitionModel ignitionModel;
          // ignitionModel.visuals.emplace_back(ignitionVisual);
          ignitionModel.pose = ignition::math::Pose3d(
              model.pose().position().x(), model.pose().position().y(),
              model.pose().position().z(), model.pose().orientation().w(),
              model.pose().orientation().x(), model.pose().orientation().y(),
              model.pose().orientation().z());
          ignitionModel.id = model.id();
          std::string sdfModelPath =
              std::string("/") + worldName + "/" + modelName;

          auto modelPrim = this->GetPrimAtPath(sdfModelPath);
          if (modelPrim)
          {
            igndbg << "Model [" << modelName
                   << "] already available in the scene" << std::endl;
            {
              std::unique_lock<std::mutex> lkPose(poseMutex);
              this->models.insert({modelName, ignitionModel});
            }
            continue;
          }

          auto usdModelXform = pxr::UsdGeomXform::Define(
              this->stage, pxr::SdfPath(sdfModelPath));
          for (auto &link : model.link())
          {
            std::string sdfLinkPath = sdfModelPath + "/" + link.name();

            for (auto &sensor : link.sensor())
            {
              std::string sdfSensorPath = sdfModelPath + "/" + link.name()
                + "/" + sensor.name();

              if (sensor.type() == "camera")
              {
                auto usdCamera = pxr::UsdGeomCamera::Define(
                  this->stage, pxr::SdfPath(sdfSensorPath));

                // TODO(ahcorde): The default value in USD is 50, but something more
                // similar to ignition Gazebo is 40.
                usdCamera.CreateFocalLengthAttr().Set(
                    static_cast<float>(52.0f));

                usdCamera.CreateClippingRangeAttr().Set(pxr::GfVec2f(
                      static_cast<float>(sensor.camera().near_clip()),
                      static_cast<float>(sensor.camera().far_clip())));
                usdCamera.CreateHorizontalApertureAttr().Set(
                  static_cast<float>(
                    sensor.camera().horizontal_fov() * 180.0f / 3.1416f));

                ignition::math::Pose3d poseCameraYUp(0, 0, 0, 1.57, 0, -1.57);
                ignition::math::Quaterniond q(
                  sensor.pose().orientation().w(),
                  sensor.pose().orientation().x(),
                  sensor.pose().orientation().y(),
                  sensor.pose().orientation().z());

                ignition::math::Pose3d poseCamera(
                  sensor.pose().position().x(),
                  sensor.pose().position().y(),
                  sensor.pose().position().z(),
                  q.Roll() * 180.0 / 3.1416,
                  q.Pitch() * 180.0 / 3.1416,
                  q.Yaw() * 180. / 3.1416);

                poseCamera = poseCamera * poseCameraYUp;

                usdCamera.AddTranslateOp(pxr::UsdGeomXformOp::Precision::PrecisionDouble)
                  .Set(
                    pxr::GfVec3d(
                      poseCamera.Pos().X(),
                      poseCamera.Pos().Y(),
                      poseCamera.Pos().Z()));

                usdCamera.AddRotateXYZOp(pxr::UsdGeomXformOp::Precision::PrecisionDouble)
                 .Set(
                    pxr::GfVec3d(
                      poseCamera.Rot().Roll() * 180.0 / 3.1416,
                      poseCamera.Rot().Pitch() * 180.0 / 3.1416,
                      poseCamera.Rot().Yaw() * 180. / 3.1416));
              }
              else if (sensor.type() == "gpu_lidar")
              {
                pxr::UsdGeomXform::Define(
                  this->stage, pxr::SdfPath(sdfSensorPath));
                auto lidarPrim = this->stage->GetPrimAtPath(
                      pxr::SdfPath(sdfSensorPath));
                lidarPrim.SetTypeName(pxr::TfToken("Lidar"));

                lidarPrim.CreateAttribute(pxr::TfToken("minRange"),
                    pxr::SdfValueTypeNames->Float, false).Set(
                      static_cast<float>(sensor.lidar().range_min()));
                lidarPrim.CreateAttribute(pxr::TfToken("maxRange"),
                    pxr::SdfValueTypeNames->Float, false).Set(
                      static_cast<float>(sensor.lidar().range_max()));
                const auto horizontalFov = sensor.lidar().horizontal_max_angle() -
                  sensor.lidar().horizontal_min_angle();
                // TODO(adlarkin) double check if these FOV calculations are correct
                lidarPrim.CreateAttribute(pxr::TfToken("horizontalFov"),
                    pxr::SdfValueTypeNames->Float, false).Set(
                      static_cast<float>(horizontalFov * 180.0f / 3.1416f));
                const auto verticalFov = sensor.lidar().vertical_max_angle() -
                  sensor.lidar().vertical_min_angle();
                lidarPrim.CreateAttribute(pxr::TfToken("verticalFov"),
                    pxr::SdfValueTypeNames->Float, false).Set(
                      static_cast<float>(verticalFov * 180.0f / 3.1416f));
                lidarPrim.CreateAttribute(pxr::TfToken("horizontalResolution"),
                    pxr::SdfValueTypeNames->Float, false).Set(
                      static_cast<float>(sensor.lidar().horizontal_resolution()));
                lidarPrim.CreateAttribute(pxr::TfToken("verticalResolution"),
                    pxr::SdfValueTypeNames->Float, false).Set(
                      static_cast<float>(sensor.lidar().vertical_resolution()));
              }
              else
              {
                  ignerr << "This kind of sensor [" << sensor.type()
                         << "] is not supported" << std::endl;
              }
            }

            auto usdLinkXform = pxr::UsdGeomXform::Define(
                this->stage, pxr::SdfPath(sdfLinkPath));

            ignition::math::Quaterniond q(
              link.pose().orientation().w(),
              link.pose().orientation().x(),
              link.pose().orientation().y(),
              link.pose().orientation().z());

            usdLinkXform.AddTranslateOp(
              pxr::UsdGeomXformOp::Precision::PrecisionDouble)
                .Set(
                  pxr::GfVec3d(
                    link.pose().position().x(),
                    link.pose().position().y(),
                    link.pose().position().z()));

            usdLinkXform.AddRotateXYZOp(
              pxr::UsdGeomXformOp::Precision::PrecisionDouble)
                .Set(
                  pxr::GfVec3d(
                    q.Roll() * 180 /3.1416,
                    q.Pitch() * 180 /3.1416,
                    q.Yaw() * 180 /3.1416));

            for (auto &visual : link.visual())
            {
              auto scene = this->SharedFromThis();
              auto ignitionVisual =
                  IgnitionVisual::Make(visual.id(), visual.name(), scene);
              ignitionVisual->AttachGeometry(visual, sdfLinkPath);

              {
                std::unique_lock<std::mutex> lkPose(poseMutex);
                this->models.insert({modelName, ignitionModel});
              }
            }
          }
        }
      }
    }
    std::this_thread::sleep_for(20ms);
  }
}

//////////////////////////////////////////////////
bool SceneImpl::SetModelPose(const std::string &_name,
                             const ignition::math::Pose3d &_pose)
{
  std::unique_lock<std::mutex> lkPose(poseMutex);
  auto it = models.find(_name);
  if (it != models.end())
  {
    it->second.pose = _pose;
    it->second.pose.Correct();
    return true;
  }
  return false;
}

bool SceneImpl::RemoveModel(const std::string &_name)
{
  return static_cast<bool>(this->models.erase(_name));
}

//////////////////////////////////////////////////
/// \brief Function called each time a topic update is received.
void SceneImpl::CallbackPoses(const ignition::msgs::Pose_V &_msg)
{
  std::unique_lock<std::mutex> lkPose(poseMutex);

  for (int i = 0; i < _msg.pose().size(); ++i)
  {
    const auto &poseMsg = _msg.pose(i);
    auto it = models.find(poseMsg.name());
    if (it != models.end())
    {
      it->second.pose = ignition::math::Pose3d(
          poseMsg.position().x(), poseMsg.position().y(),
          poseMsg.position().z(), poseMsg.orientation().w(),
          poseMsg.orientation().x(), poseMsg.orientation().y(),
          poseMsg.orientation().z());
      it->second.id = poseMsg.id();
      it->second.pose.Correct();
    }
  }
}

//////////////////////////////////////////////////
/// \brief Function called each time a topic update is received.
void SceneImpl::CallbackJoint(const ignition::msgs::Model &_msg)
{
  std::unique_lock<std::mutex> lkPose(poseMutex);

  auto it = this->models.find(_msg.name());
  if (it != this->models.end())
  {
    auto &joints = it->second.ignitionJoints;
    for (auto &joint : _msg.joint())
    {
      ignition::math::Pose3d poseJoint(
          joint.pose().position().x(), joint.pose().position().y(),
          joint.pose().position().z(), joint.pose().orientation().w(),
          joint.pose().orientation().x(), joint.pose().orientation().y(),
          joint.pose().orientation().z());

      auto itJoint = joints.find(joint.name());
      if (itJoint != joints.end())
      {
        itJoint->second->pose = poseJoint;
        itJoint->second->position = joint.axis1().position();
      }
      else
      {
        std::shared_ptr<IgnitionJoint> ignitionJoint =
            std::make_shared<IgnitionJoint>();
        ignitionJoint->pose = poseJoint;
        ignitionJoint->position = joint.axis1().position();
        joints.insert({joint.name(), ignitionJoint});
      }
    }
  }
}
}  // namespace omniverse
}  // namespace ignition
