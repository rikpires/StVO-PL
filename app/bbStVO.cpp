/*****************************************************************************
**   Stereo Visual Odometry by combining point and line segment features	**
******************************************************************************
**																			**
**	Copyright(c) 2015, Ruben Gomez-Ojeda, University of Malaga              **
**	Copyright(c) 2015, MAPIR group, University of Malaga					**
**																			**
**  This program is free software: you can redistribute it and/or modify	**
**  it under the terms of the GNU General Public License (version 3) as		**
**	published by the Free Software Foundation.								**
**																			**
**  This program is distributed in the hope that it will be useful, but		**
**	WITHOUT ANY WARRANTY; without even the implied warranty of				**
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the			**
**  GNU General Public License for more details.							**
**																			**
**  You should have received a copy of the GNU General Public License		**
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.	**
**																			**
*****************************************************************************/

#include <mutex>
#include <bumblebeeGrabber.h>
#include <stereoFrame.h>
#include <stereoFrameHandler.h>
#include <sceneRepresentation.h>

#include <ctime>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/at_c.hpp>
#include <boost/filesystem.hpp>
#include "yaml-cpp/yaml.h"

using namespace StVO;

Mat         img_l, img_r;
mutex       bb_mutex, vo_mutex;
Matrix3f    K;
float       b;
int         img_width  = 640;
int         img_height = 480;
string      frame_rate = "FRAMERATE_20";

void bumblebeeThread()
{
    // Initialize the camera
    bumblebeeGrabber bbGrabber(img_width,img_height,frame_rate);
    bbGrabber.getCalib(K,b);
    // Grab the first stereo frame
    bbGrabber.grabStereo(img_l,img_r);
    vo_mutex.unlock();
    // Start grabbing continuously
    while(true){
        bb_mutex.lock();
        bbGrabber.grabStereo(img_l,img_r);
        vo_mutex.unlock();
    }
}

void stereoVO()
{

    // setup camera
    PinholeStereoCamera* cam_pin;
    cam_pin = new PinholeStereoCamera(img_width,img_height,K(0,0),K(1,1),K(0,2),K(1,2),b);

    // create scene
    sceneRepresentation scene("scene_config.ini");
    Matrix4d Tcw, Tfw = Matrix4d::Identity(), Tfw_prev = Matrix4d::Identity(), T_inc;
    Vector6d cov_eig;
    Matrix6d cov;
    Tcw = Matrix4d::Identity();
    scene.initializeScene(Tfw);

    // initialize and run PL-StVO
    mrpt::utils::CTicTac clock;
    int frame_counter = 0;
    StereoFrameHandler* StVO = new StereoFrameHandler(cam_pin);
    while(true)
    {
        // Point-Line Tracking
        vo_mutex.lock();
        clock.Tic();
        StVO->insertStereoPair( img_l, img_r, frame_counter, T_inc );
        StVO->optimizePose();
        double t1 = 1000 * clock.Tac(); //ms

        // update scene
        scene.setText(frame_counter,t1,StVO->n_inliers_pt,StVO->matched_pt.size(),StVO->n_inliers_ls,StVO->matched_ls.size());
        scene.setCov( cov );
        scene.setPose( T_inc );
        scene.setImage( img_l );
        scene.updateScene();

        // console output
        cout.setf(ios::fixed,ios::floatfield); cout.precision(8);
        cout << "Frame: " << frame_counter << " \t Residual error: " << StVO->curr_frame->err_norm;
        cout.setf(ios::fixed,ios::floatfield); cout.precision(3);
        cout << " \t Proc. time: " << t1 << " ms\t ";
        cout << "\t Points: " << StVO->matched_pt.size() << " (" << StVO->n_inliers_pt << ") " <<
                "\t Lines:  " << StVO->matched_ls.size() << " (" << StVO->n_inliers_ls << ") " << endl;

        // update StVO
        StVO->updateFrame();

    }




}

int main(int argc, char **argv){

    vo_mutex.lock();
    bb_mutex.lock();

    thread bb(bumblebeeThread);
    thread alg(stereoVO);
    bb.join();
    alg.join();

    return 0;

}