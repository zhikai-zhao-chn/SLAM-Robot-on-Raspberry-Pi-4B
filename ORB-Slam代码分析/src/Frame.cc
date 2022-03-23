/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file Frame.cc
 * @author guoqing (1337841346@qq.com)
 * @brief 帧的实现文件
 * @version 0.1
 * @date 2019-01-03
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#include "Frame.h"
#include "Converter.h"
#include "ORBmatcher.h"
#include <thread>

namespace ORB_SLAM2
{


//下一个生成的帧的ID,这里是初始化类的静态成员变量
long unsigned int Frame::nNextId=0;

//是否要进行初始化操作的标志
//这里给这个标志置位的操作是在最初系统开始加载到内存的时候进行的，下一帧就是整个系统的第一帧，所以这个标志要置位
bool Frame::mbInitialComputations=true;

//TODO 下面这些都没有进行赋值操作，但是也写在这里，是为什么？可能仅仅是前视声明?
//目测好像仅仅是对这些类的静态成员变量做个前视声明，没有发现这个操作有特殊的含义
float Frame::cx, Frame::cy, Frame::fx, Frame::fy, Frame::invfx, Frame::invfy;
float Frame::mnMinX, Frame::mnMinY, Frame::mnMaxX, Frame::mnMaxY;
float Frame::mfGridElementWidthInv, Frame::mfGridElementHeightInv;


//无参的构造函数默认为空
Frame::Frame()
{}

/** @details 另外注意，调用这个函数的时候，这个函数中隐藏的this指针其实是指向目标帧的
 */
Frame::Frame(const Frame &frame)
    :mpORBvocabulary(frame.mpORBvocabulary), 
     mpORBextractorLeft(frame.mpORBextractorLeft), 
     mpORBextractorRight(frame.mpORBextractorRight),
     mTimeStamp(frame.mTimeStamp), 
     mK(frame.mK.clone()),									//深拷贝
     mDistCoef(frame.mDistCoef.clone()),					//深拷贝
     mbf(frame.mbf), 
     mb(frame.mb), 
     mThDepth(frame.mThDepth), 
     N(frame.N), 
     mvKeys(frame.mvKeys),									//经过实验，确定这种通过同类型对象初始化的操作是具有深拷贝的效果的
     mvKeysRight(frame.mvKeysRight), 						//深拷贝
     mvKeysUn(frame.mvKeysUn),  							//深拷贝
     mvuRight(frame.mvuRight),								//深拷贝
     mvDepth(frame.mvDepth), 								//深拷贝
     mBowVec(frame.mBowVec), 								//深拷贝
     mFeatVec(frame.mFeatVec),								//深拷贝
     mDescriptors(frame.mDescriptors.clone()), 				//cv::Mat深拷贝
     mDescriptorsRight(frame.mDescriptorsRight.clone()),	//cv::Mat深拷贝
     mvpMapPoints(frame.mvpMapPoints), 						//深拷贝
     mvbOutlier(frame.mvbOutlier), 							//深拷贝
     mnId(frame.mnId),
     mpReferenceKF(frame.mpReferenceKF), 
     mnScaleLevels(frame.mnScaleLevels),
     mfScaleFactor(frame.mfScaleFactor), 
     mfLogScaleFactor(frame.mfLogScaleFactor),
     mvScaleFactors(frame.mvScaleFactors), 					//深拷贝
     mvInvScaleFactors(frame.mvInvScaleFactors),			//深拷贝
     mvLevelSigma2(frame.mvLevelSigma2), 					//深拷贝
     mvInvLevelSigma2(frame.mvInvLevelSigma2)				//深拷贝
{
	//逐个复制，其实这里也是深拷贝
    for(int i=0;i<FRAME_GRID_COLS;i++)
        for(int j=0; j<FRAME_GRID_ROWS; j++)
			//这里没有使用前面的深拷贝方式的原因可能是mGrid是由若干vector类型对象组成的vector，
			//但是自己不知道vector内部的源码不清楚其赋值方式，在第一维度上直接使用上面的方法可能会导致
			//错误使用不合适的复制函数，导致第一维度的vector不能够被正确地“拷贝”
            mGrid[i][j]=frame.mGrid[i][j];

    if(!frame.mTcw.empty())
		//这里说的是给新的帧设置Pose
        SetPose(frame.mTcw);
}


/**
 * @brief 为双目相机准备的构造函数
 * 
 * @param[in] imLeft            左目图像
 * @param[in] imRight           右目图像
 * @param[in] timeStamp         时间戳
 * @param[in] extractorLeft     左目图像特征点提取器句柄
 * @param[in] extractorRight    右目图像特征点提取器句柄
 * @param[in] voc               ORB字典句柄
 * @param[in] K                 相机内参矩阵
 * @param[in] distCoef          相机去畸变参数
 * @param[in] bf                相机基线长度和焦距的乘积
 * @param[in] thDepth           远点和近点的深度区分阈值
 *  
 */
Frame::Frame(const cv::Mat &imLeft, const cv::Mat &imRight, const double &timeStamp, ORBextractor* extractorLeft, ORBextractor* extractorRight, ORBVocabulary* voc, cv::Mat &K, cv::Mat &distCoef, const float &bf, const float &thDepth)
    :mpORBvocabulary(voc),mpORBextractorLeft(extractorLeft),mpORBextractorRight(extractorRight), mTimeStamp(timeStamp), mK(K.clone()),mDistCoef(distCoef.clone()), mbf(bf), mThDepth(thDepth),
     mpReferenceKF(static_cast<KeyFrame*>(NULL))
{
    // Step 1 帧的ID 自增
    mnId=nNextId++;

    // Step 2 计算图像金字塔的参数 
	//获取图像金字塔的层数
    mnScaleLevels = mpORBextractorLeft->GetLevels();
	//这个是获得层与层之前的缩放比
    mfScaleFactor = mpORBextractorLeft->GetScaleFactor();
	//计算上面缩放比的对数, NOTICE log=自然对数，log10=才是以10为基底的对数 
    mfLogScaleFactor = log(mfScaleFactor);
	//获取每层图像的缩放因子
    mvScaleFactors = mpORBextractorLeft->GetScaleFactors();
	//同样获取每层图像缩放因子的倒数
    mvInvScaleFactors = mpORBextractorLeft->GetInverseScaleFactors();
	//高斯模糊的时候，使用的方差
    mvLevelSigma2 = mpORBextractorLeft->GetScaleSigmaSquares();
	//获取sigma^2的倒数
    mvInvLevelSigma2 = mpORBextractorLeft->GetInverseScaleSigmaSquares();

    // ORB extraction
    // Step 3 对左目右目图像提取ORB特征点, 第一个参数0-左图， 1-右图。为加速计算，同时开了两个线程计算
    thread threadLeft(&Frame::ExtractORB,		//该线程的主函数
					  this,						//当前帧对象的对象指针
					  0,						//表示是左图图像
					  imLeft);					//图像数据
	//对右目图像提取ORB特征，参数含义同上
    thread threadRight(&Frame::ExtractORB,this,1,imRight);
	//等待两张图像特征点提取过程完成
    threadLeft.join();
    threadRight.join();

	//mvKeys中保存的是左图像中的特征点，这里是获取左侧图像中特征点的个数
    N = mvKeys.size();

	//如果左图像中没有成功提取到特征点那么就返回，也意味这这一帧的图像无法使用
    if(mvKeys.empty())
        return;
	
    // Step 4 用OpenCV的矫正函数、内参对提取到的特征点进行矫正
    // 实际上由于双目输入的图像已经预先经过矫正,所以实际上并没有对特征点进行任何处理操作
    UndistortKeyPoints();

    // Step 5 计算双目间特征点的匹配，只有匹配成功的特征点会计算其深度,深度存放在 mvDepth 
	// mvuRight中存储的应该是左图像中的点所匹配的在右图像中的点的横坐标（纵坐标相同）
    ComputeStereoMatches();

    // 初始化本帧的地图点
    mvpMapPoints = vector<MapPoint*>(N,static_cast<MapPoint*>(NULL));   
	// 记录地图点是否为外点，初始化均为外点false
    mvbOutlier = vector<bool>(N,false);

    // This is done only for the first Frame (or after a change in the calibration)
	//  Step 5 计算去畸变后图像边界，将特征点分配到网格中。这个过程一般是在第一帧或者是相机标定参数发生变化之后进行
    if(mbInitialComputations)
    {
		//计算去畸变后图像的边界
        ComputeImageBounds(imLeft);

		// 表示一个图像像素相当于多少个图像网格列（宽）
        mfGridElementWidthInv=static_cast<float>(FRAME_GRID_COLS)/static_cast<float>(mnMaxX-mnMinX);
		// 表示一个图像像素相当于多少个图像网格行（高）
        mfGridElementHeightInv=static_cast<float>(FRAME_GRID_ROWS)/static_cast<float>(mnMaxY-mnMinY);

		//给类的静态成员变量复制
        fx = K.at<float>(0,0);
        fy = K.at<float>(1,1);
        cx = K.at<float>(0,2);
        cy = K.at<float>(1,2);
		// 猜测是因为这种除法计算需要的时间略长，所以这里直接存储了这个中间计算结果
        invfx = 1.0f/fx;
        invfy = 1.0f/fy;

		//特殊的初始化过程完成，标志复位
        mbInitialComputations=false;
    }

    // 双目相机基线长度
    mb = mbf/fx;

    // 将特征点分配到图像网格中 
    AssignFeaturesToGrid();    
}

/**
 * @brief 为RGBD相机准备的帧构造函数
 * 
 * @param[in] imGray        对RGB图像灰度化之后得到的灰度图像
 * @param[in] imDepth       深度图像
 * @param[in] timeStamp     时间戳
 * @param[in] extractor     特征点提取器句柄
 * @param[in] voc           ORB特征点词典的句柄
 * @param[in] K             相机的内参数矩阵
 * @param[in] distCoef      相机的去畸变参数
 * @param[in] bf            baseline*bf
 * @param[in] thDepth       远点和近点的深度区分阈值
 */
Frame::Frame(const cv::Mat &imGray, const cv::Mat &imDepth, const double &timeStamp, ORBextractor* extractor,ORBVocabulary* voc, cv::Mat &K, cv::Mat &distCoef, const float &bf, const float &thDepth)
    :mpORBvocabulary(voc),mpORBextractorLeft(extractor),mpORBextractorRight(static_cast<ORBextractor*>(NULL)),
     mTimeStamp(timeStamp), mK(K.clone()),mDistCoef(distCoef.clone()), mbf(bf), mThDepth(thDepth)
{
    // Step 1 帧的ID 自增
    mnId=nNextId++;

    // Step 2 计算图像金字塔的参数 
	//获取图像金字塔的层数
    mnScaleLevels = mpORBextractorLeft->GetLevels();
	//获取每层的缩放因子
    mfScaleFactor = mpORBextractorLeft->GetScaleFactor();    
	//计算每层缩放因子的自然对数
    mfLogScaleFactor = log(mfScaleFactor);
	//获取各层图像的缩放因子
    mvScaleFactors = mpORBextractorLeft->GetScaleFactors();
	//获取各层图像的缩放因子的倒数
    mvInvScaleFactors = mpORBextractorLeft->GetInverseScaleFactors();
	//TODO 也是获取这个不知道有什么实际含义的sigma^2
    mvLevelSigma2 = mpORBextractorLeft->GetScaleSigmaSquares();
	//计算上面获取的sigma^2的倒数
    mvInvLevelSigma2 = mpORBextractorLeft->GetInverseScaleSigmaSquares();

    /** 3. 提取彩色图像(其实现在已经灰度化成为灰度图像了)的特征点 \n Frame::ExtractORB() */

    // ORB extraction
	// Step 3 对图像进行提取特征点, 第一个参数0-左图， 1-右图
    ExtractORB(0,imGray);

	//获取特征点的个数
    N = mvKeys.size();

	//如果这一帧没有能够提取出特征点，那么就直接返回了
    if(mvKeys.empty())
        return;

	// Step 4 用OpenCV的矫正函数、内参对提取到的特征点进行矫正
    UndistortKeyPoints();

	// Step 5 获取图像的深度，并且根据这个深度推算其右图中匹配的特征点的视差
    ComputeStereoFromRGBD(imDepth);

    // 初始化本帧的地图点
    mvpMapPoints = vector<MapPoint*>(N,static_cast<MapPoint*>(NULL));
	// 记录地图点是否为外点，初始化均为外点false
    mvbOutlier = vector<bool>(N,false);

    // This is done only for the first Frame (or after a change in the calibration)
	//  Step 5 计算去畸变后图像边界，将特征点分配到网格中。这个过程一般是在第一帧或者是相机标定参数发生变化之后进行
    if(mbInitialComputations)
    {
		//计算去畸变后图像的边界
        ComputeImageBounds(imGray);

        // 表示一个图像像素相当于多少个图像网格列（宽）
        mfGridElementWidthInv=static_cast<float>(FRAME_GRID_COLS)/static_cast<float>(mnMaxX-mnMinX);
		// 表示一个图像像素相当于多少个图像网格行（高）
        mfGridElementHeightInv=static_cast<float>(FRAME_GRID_ROWS)/static_cast<float>(mnMaxY-mnMinY);

		//给类的静态成员变量复制
        fx = K.at<float>(0,0);
        fy = K.at<float>(1,1);
        cx = K.at<float>(0,2);
        cy = K.at<float>(1,2);
		// 猜测是因为这种除法计算需要的时间略长，所以这里直接存储了这个中间计算结果
        invfx = 1.0f/fx;
        invfy = 1.0f/fy;

		//特殊的初始化过程完成，标志复位
        mbInitialComputations=false;
    }

    // 计算假想的基线长度 baseline= mbf/fx
    // 后面要对从RGBD相机输入的特征点,结合相机基线长度,焦距,以及点的深度等信息来计算其在假想的"右侧图像"上的匹配点
    mb = mbf/fx;

	// 将特征点分配到图像网格中 
    AssignFeaturesToGrid();
}

/**
 * @brief 单目帧构造函数
 * 
 * @param[in] imGray                            //灰度图
 * @param[in] timeStamp                         //时间戳
 * @param[in & out] extractor                   //ORB特征点提取器的句柄
 * @param[in] voc                               //ORB字典的句柄
 * @param[in] K                                 //相机的内参数矩阵
 * @param[in] distCoef                          //相机的去畸变参数
 * @param[in] bf                                //baseline*f
 * @param[in] thDepth                           //区分远近点的深度阈值
 */
Frame::Frame(const cv::Mat &imGray, const double &timeStamp, ORBextractor* extractor,ORBVocabulary* voc, cv::Mat &K, cv::Mat &distCoef, const float &bf, const float &thDepth)
    :mpORBvocabulary(voc),mpORBextractorLeft(extractor),mpORBextractorRight(static_cast<ORBextractor*>(NULL)),
     mTimeStamp(timeStamp), mK(K.clone()), mDistCoef(distCoef.clone()), mbf(bf), mThDepth(thDepth)
{
    // Frame ID
	// Step 1 帧的ID 自增
    mnId=nNextId++;

    // Step 2 计算图像金字塔的参数 
    // Scale Level Info
	//获取图像金字塔的层数
    mnScaleLevels = mpORBextractorLeft->GetLevels();
	//获取每层的缩放因子
    mfScaleFactor = mpORBextractorLeft->GetScaleFactor();
	//计算每层缩放因子的自然对数
    mfLogScaleFactor = log(mfScaleFactor);
	//获取各层图像的缩放因子
    mvScaleFactors = mpORBextractorLeft->GetScaleFactors();
	//获取各层图像的缩放因子的倒数
    mvInvScaleFactors = mpORBextractorLeft->GetInverseScaleFactors();
	//获取sigma^2
    mvLevelSigma2 = mpORBextractorLeft->GetScaleSigmaSquares();
	//获取sigma^2的倒数
    mvInvLevelSigma2 = mpORBextractorLeft->GetInverseScaleSigmaSquares();

    // ORB extraction
	// Step 3 对这个单目图像进行提取特征点, 第一个参数0-左图， 1-右图
    ExtractORB(0,imGray);

	//求出特征点的个数
    N = mvKeys.size();

	//如果没有能够成功提取出特征点，那么就直接返回了
    if(mvKeys.empty())
        return;

    // Step 4 用OpenCV的矫正函数、内参对提取到的特征点进行矫正 
    UndistortKeyPoints();

    // Set no stereo information
	// 由于单目相机无法直接获得立体信息，所以这里要给右图像对应点和深度赋值-1表示没有相关信息
    mvuRight = vector<float>(N,-1);
    mvDepth = vector<float>(N,-1);


    // 初始化本帧的地图点
    mvpMapPoints = vector<MapPoint*>(N,static_cast<MapPoint*>(NULL));
	// 记录地图点是否为外点，初始化均为外点false
    mvbOutlier = vector<bool>(N,false);

    // This is done only for the first Frame (or after a change in the calibration)
	//  Step 5 计算去畸变后图像边界，将特征点分配到网格中。这个过程一般是在第一帧或者是相机标定参数发生变化之后进行
    if(mbInitialComputations)
    {
		// 计算去畸变后图像的边界
        ComputeImageBounds(imGray);

		// 表示一个图像像素相当于多少个图像网格列（宽）
        mfGridElementWidthInv=static_cast<float>(FRAME_GRID_COLS)/static_cast<float>(mnMaxX-mnMinX);
		// 表示一个图像像素相当于多少个图像网格行（高）
        mfGridElementHeightInv=static_cast<float>(FRAME_GRID_ROWS)/static_cast<float>(mnMaxY-mnMinY);

		//给类的静态成员变量复制
        fx = K.at<float>(0,0);
        fy = K.at<float>(1,1);
        cx = K.at<float>(0,2);
        cy = K.at<float>(1,2);
		// 猜测是因为这种除法计算需要的时间略长，所以这里直接存储了这个中间计算结果
        invfx = 1.0f/fx;
        invfy = 1.0f/fy;

		//特殊的初始化过程完成，标志复位
        mbInitialComputations=false;
    }

    //计算 basline
    mb = mbf/fx;

	// 将特征点分配到图像网格中 
    AssignFeaturesToGrid();
}

/**
 * @brief 将提取的ORB特征点分配到图像网格中
 * 
 */
void Frame::AssignFeaturesToGrid()
{
    // Step 1  给存储特征点的网格数组 Frame::mGrid 预分配空间
	// ? 这里0.5 是为什么？节省空间？
    // FRAME_GRID_COLS = 64，FRAME_GRID_ROWS=48
    int nReserve = 0.5f*N/(FRAME_GRID_COLS*FRAME_GRID_ROWS);
	//开始对mGrid这个二维数组中的每一个vector元素遍历并预分配空间
    for(unsigned int i=0; i<FRAME_GRID_COLS;i++)
        for (unsigned int j=0; j<FRAME_GRID_ROWS;j++)
            mGrid[i][j].reserve(nReserve);

    // Step 2 遍历每个特征点，将每个特征点在mvKeysUn中的索引值放到对应的网格mGrid中
    for(int i=0;i<N;i++)
    {
		//从类的成员变量中获取已经去畸变后的特征点
        const cv::KeyPoint &kp = mvKeysUn[i];

		//存储某个特征点所在网格的网格坐标，nGridPosX范围：[0,FRAME_GRID_COLS], nGridPosY范围：[0,FRAME_GRID_ROWS]
        int nGridPosX, nGridPosY;
		// 计算某个特征点所在网格的网格坐标，如果找到特征点所在的网格坐标，记录在nGridPosX,nGridPosY里，返回true，没找到返回false
        if(PosInGrid(kp,nGridPosX,nGridPosY))
			//如果找到特征点所在网格坐标，将这个特征点的索引添加到对应网格的数组mGrid中
            mGrid[nGridPosX][nGridPosY].push_back(i);
    }
}

/**
 * @brief 提取图像的ORB特征点，提取的关键点存放在mvKeys，描述子存放在mDescriptors
 * 
 * @param[in] flag          标记是左图还是右图。0：左图  1：右图
 * @param[in] im            等待提取特征点的图像
 */
void Frame::ExtractORB(int flag, const cv::Mat &im)
{
    // 判断是左图还是右图
    if(flag==0)
        // 左图的话就套使用左图指定的特征点提取器，并将提取结果保存到对应的变量中 
        // 这里使用了仿函数来完成，重载了括号运算符 ORBextractor::operator() 
        (*mpORBextractorLeft)(im,				//待提取特征点的图像
							  cv::Mat(),		//掩摸图像, 实际没有用到
							  mvKeys,			//输出变量，用于保存提取后的特征点
							  mDescriptors);	//输出变量，用于保存特征点的描述子
    else
        // 右图的话就需要使用右图指定的特征点提取器，并将提取结果保存到对应的变量中 
        (*mpORBextractorRight)(im,cv::Mat(),mvKeysRight,mDescriptorsRight);
}

// 设置相机姿态，随后会调用 UpdatePoseMatrices() 来改变mRcw,mRwc等变量的值
void Frame::SetPose(cv::Mat Tcw)
{
	/** 1. 更改类的成员变量,深拷贝 */
    mTcw = Tcw.clone();
	/** 2. 调用 Frame::UpdatePoseMatrices() 来更新、计算类的成员变量中所有的位姿矩阵 */
    UpdatePoseMatrices();
}

//根据Tcw计算mRcw、mtcw和mRwc、mOw
void Frame::UpdatePoseMatrices()
{
    /** 主要计算四个量. 定义程序中的符号和公式表达: \n
     * Frame::mTcw = \f$ \mathbf{T}_{cw} \f$ \n
     * Frame::mRcw = \f$ \mathbf{R}_{cw} \f$ \n
     * Frame::mRwc = \f$ \mathbf{R}_{wc} \f$ \n
     * Frame::mtcw = \f$ \mathbf{t}_{cw} \f$ 即相机坐标系下相机坐标系到世界坐标系间的向量, 向量方向由相机坐标系指向世界坐标系\n
     * Frame::mOw  = \f$ \mathbf{O}_{w} \f$  即世界坐标系下世界坐标系到相机坐标系间的向量, 向量方向由世界坐标系指向相机坐标系\n  
     * 步骤: */
    /** 1. 计算mRcw,即相机从世界坐标系到当前帧的相机位置的旋转. \n
     * 这里是直接从 Frame::mTcw 中提取出旋转矩阵. \n*/
    // [x_camera 1] = [R|t]*[x_world 1]，坐标为齐次形式
    // x_camera = R*x_world + t
	//注意，rowRange这个只取到范围的左边界，而不取右边界
	//所以下面这个其实就是从变换矩阵中提取出旋转矩阵
    mRcw = mTcw.rowRange(0,3).colRange(0,3);
    /** 2. 相反的旋转就是取个逆，对于正交阵也就是取个转置: \n
     * \f$ \mathbf{R}_{wc}=\mathbf{R}_{cw}^{-1}=\mathbf{R}_{cw}^{\text{T}} \f$ \n
     * 得到 mRwc .
     */
    mRwc = mRcw.t();
	/** 3. 同样地，从变换矩阵 \f$ \mathbf{T}_{cw} \f$中提取出平移向量 \f$ \mathbf{t}_{cw} \f$ \n 
     * 进而得到 mtcw. 
     */
    mtcw = mTcw.rowRange(0,3).col(3);
    // mtcw, 即相机坐标系下相机坐标系到世界坐标系间的向量, 向量方向由相机坐标系指向世界坐标系
    // mOw, 即世界坐标系下世界坐标系到相机坐标系间的向量, 向量方向由世界坐标系指向相机坐标系

    //可能又错误,不要看接下来的这一段!!!
	//其实上面这两个平移向量应当描述的是两个坐标系原点之间的相互位置，mOw也就是相机的中心位置吧（在世界坐标系下）
	//上面的两个量互为相反的关系,但是由于mtcw这个向量是在相机坐标系下来说的，所以要反旋转变换到世界坐标系下，才能够表示mOw

    /** 4. 最终求得相机光心在世界坐标系下的坐标: \n
     * \f$ \mathbf{O}_w=-\mathbf{R}_{cw}^{\text{T}}\mathbf{t}_{cw} \f$ \n
     * 使用这个公式的原因可以按照下面的思路思考. 假设我们有了一个点 \f$ \mathbf{P} \f$ ,有它在世界坐标系下的坐标 \f$ \mathbf{P}_w \f$ 
     * 和在当前帧相机坐标系下的坐标 \f$ \mathbf{P}_c \f$ ,那么比较容易有: \n
     * \f$ \mathbf{P}_c=\mathbf{R}_{cw}\mathbf{P}_w+\mathbf{t}_{cw}  \f$ \n
     * 移项: \n
     * \f$ \mathbf{P}_c-\mathbf{t}_{cw}=\mathbf{R}_{cw}\mathbf{P}_w \f$ \n
     * 移项,考虑到旋转矩阵为正交矩阵,其逆等于其转置: \n
     * \f$ \mathbf{R}_{cw}^{-1}\left(\mathbf{P}_c-\mathbf{t}_{cw}\right)=
     * \mathbf{R}_{cw}^{\text{T}}\left(\mathbf{P}_c-\mathbf{t}_{cw}\right) = 
     * \mathbf{P}_w \f$ \n
     * 此时,如果点\f$ \mathbf{P} \f$就是表示相机光心的话,那么这里的\f$ \mathbf{P}_w \f$也就是相当于 \f$ \mathbf{O}_w \f$ 了,
     * 并且形象地可以知道 \f$ \mathbf{P}_c=0 \f$. 所以上面的式子就变成了: \n
     * \f$ \mathbf{O}_w=\mathbf{P}_w=\left(-\mathbf{t}_{cw}\right) \f$ \n
     * 于是就有了程序中的计算的公式. \n
     * 也许你会想说为什么不是 \f$ \mathbf{O}_w=-\mathbf{t}_{cw} \f$,是因为如果这样做的话没有考虑到坐标系之间的旋转.
     */ 
	
    mOw = -mRcw.t()*mtcw;

	/* 下面都是之前的推导,可能有错误,不要看!!!!!
	其实上面的算式可以写成下面的形式：
	mOw=(Rcw')*(-tcw)*[0,0,0]'  (MATLAB写法)（但是这里的计算步骤貌似是不对的）
	这里的下标有些意思，如果倒着读，就是“坐标系下点的坐标变化”，如果正着读，就是“坐标系本身的变化”，正好两者互逆
	所以这里就有：
	tcw:相机坐标系到世界坐标系的平移向量
	-tcw:世界坐标系到相机坐标系的平移向量
	Rcw：相机坐标系到世界坐标系所发生的旋转
	Rcw^t=Rcw^-1:世界坐标系到相机坐标系所发生的旋转
	最后一个因子则是世界坐标系的原点坐标
	不过这样一来，如果是先旋转后平移的话，上面的计算公式就是这个了：
	mOw=(Rcw')*[0,0,0]'+(-tcw)
	唉也确实是，对于一个原点无论发生什么样的旋转，其坐标还是不变啊。有点懵逼。
	会不会是这样：在讨论坐标系变换问题的时候，我们处理的是先平移，再旋转？如果是这样的话：
	mOw=(Rcw')*([0,0,0]'+(-tcw))=(Rcw')*(-tcw)
	就讲得通了
	新问题：这里的mOw和-tcw又是什么关系呢?目前来看好似是前者考虑了旋转而后者没有（后来的想法证明确实是这样的）
	另外还有一种理解方法：
	Pc=Rcw*Pw+tcw
	Pc-tcw=Rcw*Pw
	Rcw'(Pc-tcw)=Pw		然后前面是对点在不同坐标系下的坐标变换，当计算坐标系本身的变化的时候反过来，
	(这一行的理解不正确)将Pc认为是世界坐标系原点坐标，Pw认为是相机的光心坐标
    应该这样说,当P点就是相机光心的时候,Pw其实就是这里要求的mOw,Pc明显=0
	Rcw'(o-tcw)=c
	c=Rcw'*(-tcw)			就有了那个式子
	**/
}

// 判断路标点是否在视野中
bool Frame::isInFrustum(MapPoint *pMP, float viewingCosLimit)
{
    /** 步骤: \n <ul>*/
    /**  <li> 1.默认设置标志 MapPoint::mbTrackInView 为否,即设置该地图点不进行重投影. </li>\n
     * mbTrackInView是决定一个地图点是否进行重投影的标志，这个标志的确定要经过多个函数的确定，isInFrustum()只是其中的一个 
     * 验证关卡。这里默认设置为否. \n
     */
    pMP->mbTrackInView = false;

    // 3D in absolute coordinates
    /** <li> 2.获得这个地图点的世界坐标, 使用 MapPoint::GetWorldPos() 来获得。</li>\n*/
    cv::Mat P = pMP->GetWorldPos(); 

    // 3D in camera coordinates
    /** <li> 3.然后根据 Frame::mRcw 和 Frame::mtcw 计算这个点\f$\mathbf{P}\f$在当前相机坐标系下的坐标: </li>\n
     * \f$ \mathbf{R}_{cw}\mathbf{P}+\mathbf{t}_{cw} \f$ \n
     * 并提取出三个坐标的坐标值.
     */ 
    
    const cv::Mat Pc = mRcw*P+mtcw; // 这里的Rt是经过初步的优化后的
    //然后提取出三个坐标的坐标值
    const float &PcX = Pc.at<float>(0);
    const float &PcY = Pc.at<float>(1);
    const float &PcZ = Pc.at<float>(2);

    // Check positive depth
    /** <li> 4. <b>关卡一</b>：检查这个地图点在当前帧的相机坐标系下，是否有正的深度.如果是负的，就说明它在当前帧下不在相机视野中，也无法在当前帧下进行重投影. </li>*/
    if(PcZ<0.0f)
        return false;

    // Project in image and check it is not outside
    /** <li> 5. <b>关卡二</b>：将MapPoint投影到当前帧, 并判断是否在图像内（即是否在图像边界中）。</li>\n
     * 投影方程： \n
     * \f$ \begin{cases}
     * z^{-1} &= \frac{1}{\mathbf{P}_c.Z} \\
     * u &= z^{-1} f_x \mathbf{P}_c.X + c_x \\
     * v &= z^{-1} f_y \mathbf{P}_c.Y + c_y 
     * \end{cases} \f$
     */ 
    // V-D 1) 将MapPoint投影到当前帧, 并判断是否在图像内
    const float invz = 1.0f/PcZ;			//1/Z，其实这个Z在当前的相机坐标系下的话，就是这个点到相机光心的距离，也就是深度
    const float u=fx*PcX*invz+cx;			//计算投影在当前帧图像上的像素横坐标
    const float v=fy*PcY*invz+cy;			//计算投影在当前帧图像上的像素纵坐标

    //判断是否在图像边界中，只要不在那么就说明无法在当前帧下进行重投影
    if(u<mnMinX || u>mnMaxX)
        return false;
    if(v<mnMinY || v>mnMaxY)
        return false;

    // Check distance is in the scale invariance region of the MapPoint
    // V-D 3) 计算MapPoint到相机中心的距离, 并判断是否在尺度变化的距离内
    /** <li> 6. <b>关卡三</b>：计算MapPoint到相机中心的距离, 并判断是否在尺度变化的距离内 </li>  \n
     * 这里所说的尺度变化是指地图点到相机中心距离的一段范围，如果计算出的地图点到相机中心距离不在这个范围的话就认为这个点在 
     * 当前帧相机位姿下不能够得到正确、有效、可靠的观测，就要跳过. \n
     * 为了完成这个任务有两个子任务：\n <ul>
     */ 
     
     /** <li> 6.1 得到认为的可靠距离范围。</li> \n
     *  这个距离的上下限分别通过 MapPoint::GetMaxDistanceInvariance() 和 MapPoint::GetMinDistanceInvariance() 来得到。 */
    const float maxDistance = pMP->GetMaxDistanceInvariance();
    const float minDistance = pMP->GetMinDistanceInvariance();

    /** <li> 6.2 得到当前3D地图点距离当前帧相机光心的距离。</li> \n
     * 具体实现上是通过构造3D点P到相机光心的向量 \f$\mathbf{P}_0 \f$ ，通过对向量取模即可得到距离\f$dist\f$。
     * </ul>
     */
    const cv::Mat PO = P-mOw;
	//取模就得到了距离
    const float dist = cv::norm(PO);

	//如果不在允许的尺度变化范围内，认为重投影不可靠
    if(dist<minDistance || dist>maxDistance)
        return false;

    // Check viewing angle
    // V-D 2) 计算当前视角和平均视角夹角的余弦值, 若小于cos(60), 即夹角大于60度则返回
    /** <li> 7. <b>关卡四</b>：计算当前视角和平均视角夹角的余弦值, 若小于cos(60), 即夹角大于60度则返回 </li>     
     * <ul>
     */ 
    /** <li> 7.1 使用 MapPoint::GetNormal() 来获得平均视角(其实是一个单位向量\f$ \mathbf{P}_n \f$ ) </li> */
	//获取平均视角，目测这个平均视角只是一个方向向量，模长为1，它表示了当前帧下观测到的点的分布情况
	//TODO 但是这个平均视角是在mapoint.cpp中计算的，还不是很清楚这个具体含义  其实现在我觉得就是普通的视角的理解吧
    cv::Mat Pn = pMP->GetNormal();

	/** <li> 7.2 计算当前视角和平均视角夹角的余弦值，注意平均视角为单位向量 </li>  \n
     * 其实就是初中学的计算公式： \n
     * \f$  viewCos= {\mathbf{P}_0 \cdot \mathbf{P}_n }/{dist} \f$
    */
    const float viewCos = PO.dot(Pn)/dist;

    /** <li> 7.3 然后判断视角是否超过阈值即可。 </li></ul> */
	//如果大于规定的阈值，认为这个点太偏了，重投影不可靠，返回
    if(viewCos<viewingCosLimit)
        return false;

    // Predict scale in the image
    // V-D 4) 根据深度预测尺度（对应特征点在一层）
    /** <li> 8. 经过了上面的重重考验，说明这个地图点可以被重投影了。接下来需要记录关于这个地图点的一些信息：</li> <ul>*/
     
    //注意在特征点提取的过程中,图像金字塔的不同的层代表着特征点的不同的尺度
    // 由于地图点与特征点没有匹配关系，所以地图点对应的图像金字塔的尺度信息需要预测。
    /** <li> 8.1 使用 MapPoint::PredictScale() 来预测该地图点在现有距离 \f$ dist \f$ 下时，在当前帧
     * 的图像金字塔中，所可能对应的尺度。\n
     * 其实也就是预测可能会在哪一层。这个信息将会被保存在这个地图点对象的 MapPoint::mnTrackScaleLevel 中。
    */
    const int nPredictedLevel = pMP->PredictScale(dist,		//这个点到光心的距离
												  this);	//给出这个帧

    // Data used by the tracking	
    /** <li> 8.2 通过置位标记 MapPoint::mbTrackInView 来表示这个地图点要被投影 </li> */
    pMP->mbTrackInView = true;	
    /** <li> 8.3 然后计算这个点在左侧图像和右侧图像中的横纵坐标。 </li> 
     * 地图点在当前帧中被追踪到的横纵坐标其实就是其投影在当前帧上的像素坐标 u,v => MapPoint::mTrackProjX,MapPoint::mTrackProjY \n
     * 其中在右侧图像上的横坐标 MapPoint::mTrackProjXR 按照公式计算：\n
     * \f$ X_R=u-z^{-1} \cdot mbf \f$ \n
     * 其来源参考SLAM十四讲的P51式5.16。
    */
    pMP->mTrackProjX = u;				//该地图点投影在左侧图像的像素横坐标
    pMP->mTrackProjXR = u - mbf*invz; 	//bf/z其实是视差，为了求右图像中对应点的横坐标就得这样减了～
										//这里确实直接使用mbf计算会非常方便
    pMP->mTrackProjY = v;				//该地图点投影在左侧图像的像素纵坐标

    pMP->mnTrackScaleLevel = nPredictedLevel;		//TODO 根据上面的计算来看是存储了根据深度预测的尺度，但是有什么用不知道
    /** <li> 8.4 保存当前视角和平均视角夹角的余弦值 </li></ul>*/
    pMP->mTrackViewCos = viewCos;					

    //执行到这里说明这个地图点在相机的视野中并且进行重投影是可靠的，返回true
    return true;

    /** </ul> */
}

/**
 * @brief 找到在 以x,y为中心,半径为r的圆形内且金字塔层级在[minLevel, maxLevel]的特征点
 * 
 * @param[in] x                     特征点坐标x
 * @param[in] y                     特征点坐标y
 * @param[in] r                     搜索半径 
 * @param[in] minLevel              最小金字塔层级
 * @param[in] maxLevel              最大金字塔层级
 * @return vector<size_t>           返回搜索到的候选匹配点id
 */
vector<size_t> Frame::GetFeaturesInArea(const float &x, const float  &y, const float  &r, const int minLevel, const int maxLevel) const
{
	// 存储搜索结果的vector
    vector<size_t> vIndices;
    vIndices.reserve(N);

    // Step 1 计算半径为r圆左右上下边界所在的网格列和行的id
    // 查找半径为r的圆左侧边界所在网格列坐标。这个地方有点绕，慢慢理解下：
    // (mnMaxX-mnMinX)/FRAME_GRID_COLS：表示列方向每个网格可以平均分得几个像素（肯定大于1）
    // mfGridElementWidthInv=FRAME_GRID_COLS/(mnMaxX-mnMinX) 是上面倒数，表示每个像素可以均分几个网格列（肯定小于1）
	// (x-mnMinX-r)，可以看做是从图像的左边界mnMinX到半径r的圆的左边界区域占的像素列数
	// 两者相乘，就是求出那个半径为r的圆的左侧边界在哪个网格列中
    // 保证nMinCellX 结果大于等于0
    const int nMinCellX = max(0,(int)floor( (x-mnMinX-r)*mfGridElementWidthInv));


	// 如果最终求得的圆的左边界所在的网格列超过了设定了上限，那么就说明计算出错，找不到符合要求的特征点，返回空vector
    if(nMinCellX>=FRAME_GRID_COLS)
        return vIndices;

	// 计算圆所在的右边界网格列索引
    const int nMaxCellX = min((int)FRAME_GRID_COLS-1, (int)ceil((x-mnMinX+r)*mfGridElementWidthInv));
	// 如果计算出的圆右边界所在的网格不合法，说明该特征点不好，直接返回空vector
    if(nMaxCellX<0)
        return vIndices;

	//后面的操作也都是类似的，计算出这个圆上下边界所在的网格行的id
    const int nMinCellY = max(0,(int)floor((y-mnMinY-r)*mfGridElementHeightInv));
    if(nMinCellY>=FRAME_GRID_ROWS)
        return vIndices;

    const int nMaxCellY = min((int)FRAME_GRID_ROWS-1,(int)ceil((y-mnMinY+r)*mfGridElementHeightInv));
    if(nMaxCellY<0)
        return vIndices;

    // 检查需要搜索的图像金字塔层数范围是否符合要求
    //? 疑似bug。(minLevel>0) 后面条件 (maxLevel>=0)肯定成立
    //? 改为 const bool bCheckLevels = (minLevel>=0) || (maxLevel>=0);
    const bool bCheckLevels = (minLevel>0) || (maxLevel>=0);

    // Step 2 遍历圆形区域内的所有网格，寻找满足条件的候选特征点，并将其index放到输出里
    for(int ix = nMinCellX; ix<=nMaxCellX; ix++)
    {
        for(int iy = nMinCellY; iy<=nMaxCellY; iy++)
        {
            // 获取这个网格内的所有特征点在 Frame::mvKeysUn 中的索引
            const vector<size_t> vCell = mGrid[ix][iy];
			// 如果这个网格中没有特征点，那么跳过这个网格继续下一个
            if(vCell.empty())
                continue;

            // 如果这个网格中有特征点，那么遍历这个图像网格中所有的特征点
            for(size_t j=0, jend=vCell.size(); j<jend; j++)
            {
				// 根据索引先读取这个特征点 
                const cv::KeyPoint &kpUn = mvKeysUn[vCell[j]];
				// 保证给定的搜索金字塔层级范围合法
                if(bCheckLevels)
                {
					// cv::KeyPoint::octave中表示的是从金字塔的哪一层提取的数据
					// 保证特征点是在金字塔层级minLevel和maxLevel之间，不是的话跳过
                    if(kpUn.octave<minLevel)
                        continue;
                    if(maxLevel>=0)		//? 为何特意又强调？感觉多此一举
                        if(kpUn.octave>maxLevel)
                            continue;
                }               

                // 通过检查，计算候选特征点到圆中心的距离，查看是否是在这个圆形区域之内
                const float distx = kpUn.pt.x-x;
                const float disty = kpUn.pt.y-y;

				// 如果x方向和y方向的距离都在指定的半径之内，存储其index为候选特征点
                if(fabs(distx)<r && fabs(disty)<r)
                    vIndices.push_back(vCell[j]);
            }
        }
    }
    return vIndices;
}


/**
 * @brief 计算某个特征点所在网格的网格坐标，如果找到特征点所在的网格坐标，记录在nGridPosX,nGridPosY里，返回true，没找到返回false
 * 
 * @param[in] kp                    给定的特征点
 * @param[in & out] posX            特征点所在网格坐标的横坐标
 * @param[in & out] posY            特征点所在网格坐标的纵坐标
 * @return true                     如果找到特征点所在的网格坐标，返回true
 * @return false                    没找到返回false
 */
bool Frame::PosInGrid(const cv::KeyPoint &kp, int &posX, int &posY)
{
	// 计算特征点x,y坐标落在哪个网格内，网格坐标为posX，posY
    // mfGridElementWidthInv=(FRAME_GRID_COLS)/(mnMaxX-mnMinX);
    // mfGridElementHeightInv=(FRAME_GRID_ROWS)/(mnMaxY-mnMinY);
    posX = round((kp.pt.x-mnMinX)*mfGridElementWidthInv);
    posY = round((kp.pt.y-mnMinY)*mfGridElementHeightInv);

    //Keypoint's coordinates are undistorted, which could cause to go out of the image
    // 因为特征点进行了去畸变，而且前面计算是round取整，所以有可能得到的点落在图像网格坐标外面
    // 如果网格坐标posX，posY超出了[0,FRAME_GRID_COLS] 和[0,FRAME_GRID_ROWS]，表示该特征点没有对应网格坐标，返回false
    if(posX<0 || posX>=FRAME_GRID_COLS || posY<0 || posY>=FRAME_GRID_ROWS)
        return false;

	// 计算成功返回true
    return true;
}

//计算词包 mBowVec 和 mFeatVec
void Frame::ComputeBoW()
{
	
    /** 这个函数只有在当前帧的词袋是空的时候才回进行操作。步骤如下:<ul> */
    if(mBowVec.empty())
    {
		// 将描述子mDescriptors转换为DBOW要求的输入格式
        vector<cv::Mat> vCurrentDesc = Converter::toDescriptorVector(mDescriptors);
		// 将特征点的描述子转换成词袋向量mBowVec以及特征向量mFeatVec
        mpORBvocabulary->transform(vCurrentDesc,	//当前的描述子vector
								   mBowVec,			//输出，词袋向量，记录的是单词的id及其对应权重TF-IDF值
								   mFeatVec,		//输出，记录node id及其对应的图像 feature对应的索引
								   4);				//4表示从叶节点向前数的层数
    }
    /** </ul> */
}

/**
 * @brief 用内参对特征点去畸变，结果报存在mvKeysUn中
 * 
 */
void Frame::UndistortKeyPoints()
{
    // Step 1 如果第一个畸变参数为0，不需要矫正。第一个畸变参数k1是最重要的，一般不为0，为0的话，说明畸变参数都是0
	//变量mDistCoef中存储了opencv指定格式的去畸变参数，格式为：(k1,k2,p1,p2,k3)
    if(mDistCoef.at<float>(0)==0.0)
    {
        mvKeysUn=mvKeys;
        return;
    }


    // Step 2 如果畸变参数不为0，用OpenCV函数进行畸变矫正
    // Fill matrix with points
    // N为提取的特征点数量，为满足OpenCV函数输入要求，将N个特征点保存在N*2的矩阵中
    cv::Mat mat(N,2,CV_32F);
	//遍历每个特征点，并将它们的坐标保存到矩阵中
    for(int i=0; i<N; i++)
    {
		//然后将这个特征点的横纵坐标分别保存
        mat.at<float>(i,0)=mvKeys[i].pt.x;
        mat.at<float>(i,1)=mvKeys[i].pt.y;
    }

    // Undistort points
    // 函数reshape(int cn,int rows=0) 其中cn为更改后的通道数，rows=0表示这个行将保持原来的参数不变
    //为了能够直接调用opencv的函数来去畸变，需要先将矩阵调整为2通道（对应坐标x,y） 
    mat=mat.reshape(2);
    cv::undistortPoints(	
		mat,				//输入的特征点坐标
		mat,				//输出的校正后的特征点坐标覆盖原矩阵
		mK,					//相机的内参数矩阵
		mDistCoef,			//相机畸变参数矩阵
		cv::Mat(),			//一个空矩阵，对应为函数原型中的R
		mK); 				//新内参数矩阵，对应为函数原型中的P
	
	//调整回只有一个通道，回归我们正常的处理方式
    mat=mat.reshape(1);

    // Fill undistorted keypoint vector
    // Step 存储校正后的特征点
    mvKeysUn.resize(N);
	//遍历每一个特征点
    for(int i=0; i<N; i++)
    {
		//根据索引获取这个特征点
		//注意之所以这样做而不是直接重新声明一个特征点对象的目的是，能够得到源特征点对象的其他属性
        cv::KeyPoint kp = mvKeys[i];
		//读取校正后的坐标并覆盖老坐标
        kp.pt.x=mat.at<float>(i,0);
        kp.pt.y=mat.at<float>(i,1);
        mvKeysUn[i]=kp;
    }
}

/**
 * @brief 计算去畸变图像的边界
 * 
 * @param[in] imLeft            需要计算边界的图像
 */
void Frame::ComputeImageBounds(const cv::Mat &imLeft)	
{
    // 如果畸变参数不为0，用OpenCV函数进行畸变矫正
    if(mDistCoef.at<float>(0)!=0.0)
	{
        // 保存矫正前的图像四个边界点坐标： (0,0) (cols,0) (0,rows) (cols,rows)
        cv::Mat mat(4,2,CV_32F);
        mat.at<float>(0,0)=0.0;         //左上
		mat.at<float>(0,1)=0.0;
        mat.at<float>(1,0)=imLeft.cols; //右上
		mat.at<float>(1,1)=0.0;
		mat.at<float>(2,0)=0.0;         //左下
		mat.at<float>(2,1)=imLeft.rows;
        mat.at<float>(3,0)=imLeft.cols; //右下
		mat.at<float>(3,1)=imLeft.rows;

        // Undistort corners
		// 和前面校正特征点一样的操作，将这几个边界点作为输入进行校正
        mat=mat.reshape(2);
        cv::undistortPoints(mat,mat,mK,mDistCoef,cv::Mat(),mK);
        mat=mat.reshape(1);

		//校正后的四个边界点已经不能够围成一个严格的矩形，因此在这个四边形的外侧加边框作为坐标的边界
        mnMinX = min(mat.at<float>(0,0),mat.at<float>(2,0));//左上和左下横坐标最小的
        mnMaxX = max(mat.at<float>(1,0),mat.at<float>(3,0));//右上和右下横坐标最大的
        mnMinY = min(mat.at<float>(0,1),mat.at<float>(1,1));//左上和右上纵坐标最小的
        mnMaxY = max(mat.at<float>(2,1),mat.at<float>(3,1));//左下和右下纵坐标最小的
    }
    else
    {
        // 如果畸变参数为0，就直接获得图像边界
        mnMinX = 0.0f;
        mnMaxX = imLeft.cols;
        mnMinY = 0.0f;
        mnMaxY = imLeft.rows;
    }
}

/*
 * 双目匹配函数
 *
 * 为左图的每一个特征点在右图中找到匹配点 \n
 * 根据基线(有冗余范围)上描述子距离找到匹配, 再进行SAD精确定位 \n ‘
 * 这里所说的SAD是一种双目立体视觉匹配算法，可参考[https://blog.csdn.net/u012507022/article/details/51446891]
 * 最后对所有SAD的值进行排序, 剔除SAD值较大的匹配对，然后利用抛物线拟合得到亚像素精度的匹配 \n 
 * 这里所谓的亚像素精度，就是使用这个拟合得到一个小于一个单位像素的修正量，这样可以取得更好的估计结果，计算出来的点的深度也就越准确
 * 匹配成功后会更新 mvuRight(ur) 和 mvDepth(Z)
 */
void Frame::ComputeStereoMatches()
{
    /*两帧图像稀疏立体匹配（即：ORB特征点匹配，非逐像素的密集匹配，但依然满足行对齐）
     * 输入：两帧立体矫正后的图像img_left 和 img_right 对应的orb特征点集
     * 过程：
          1. 行特征点统计. 统计img_right每一行上的ORB特征点集，便于使用立体匹配思路(行搜索/极线搜索）进行同名点搜索, 避免逐像素的判断.
          2. 粗匹配. 根据步骤1的结果，对img_left第i行的orb特征点pi，在img_right的第i行上的orb特征点集中搜索相似orb特征点, 得到qi
          3. 精确匹配. 以点qi为中心，半径为r的范围内，进行块匹配（归一化SAD），进一步优化匹配结果
          4. 亚像素精度优化. 步骤3得到的视差为uchar/int类型精度，并不一定是真实视差，通过亚像素差值（抛物线插值)获取float精度的真实视差
          5. 最优视差值/深度选择. 通过胜者为王算法（WTA）获取最佳匹配点。
          6. 删除离缺点(outliers). 块匹配相似度阈值判断，归一化sad最小，并不代表就一定是正确匹配，比如光照变化、弱纹理等会造成误匹配
     * 输出：稀疏特征点视差图/深度图（亚像素精度）mvDepth 匹配结果 mvuRight
     */

    // 为匹配结果预先分配内存，数据类型为float型
    // mvuRight存储右图匹配点索引
    // mvDepth存储特征点的深度信息
	mvuRight = vector<float>(N,-1.0f);
    mvDepth = vector<float>(N,-1.0f);

	// orb特征相似度阈值  -> mean ～= (max  + min) / 2
    const int thOrbDist = (ORBmatcher::TH_HIGH+ORBmatcher::TH_LOW)/2;

    // 金字塔顶层（0层）图像高 nRows
    const int nRows = mpORBextractorLeft->mvImagePyramid[0].rows;

	// 二维vector存储每一行的orb特征点的列坐标，为什么是vector，因为每一行的特征点有可能不一样，例如
    // vRowIndices[0] = [1，2，5，8, 11]   第1行有5个特征点,他们的列号（即x坐标）分别是1,2,5,8,11
    // vRowIndices[1] = [2，6，7，9, 13, 17, 20]  第2行有7个特征点.etc
    vector<vector<size_t> > vRowIndices(nRows, vector<size_t>());
    for(int i=0; i<nRows; i++） vRowIndices[i].reserve(200);

	// 右图特征点数量，N表示数量 r表示右图，且不能被修改
    const int Nr = mvKeysRight.size();

	// Step 1. 行特征点统计. 考虑到尺度金字塔特征，一个特征点可能存在于多行，而非唯一的一行
    for(int iR = 0; iR < Nr; iR++) {

        // 获取特征点ir的y坐标，即行号
        const cv::KeyPoint &kp = mvKeysRight[iR];
        const float &kpY = kp.pt.y;
        
        // 计算特征点ir在行方向上，可能的偏移范围r，即可能的行号为[kpY + r, kpY -r]
        // 2 表示在全尺寸(scale = 1)的情况下，假设有2个像素的偏移，随着尺度变化，r也跟着变化
        const float r = 2.0f * mvScaleFactors[mvKeysRight[iR].octave];
        const int maxr = ceil(kpY + r);
        const int minr = floor(kpY - r);

        // 将特征点ir保证在可能的行号中
        for(int yi=minr;yi<=maxr;yi++)
            vRowIndices[yi].push_back(iR);
    }

    // Step 2 -> 3. 粗匹配 + 精匹配
    // 对于立体矫正后的两张图，在列方向(x)存在最大视差maxd和最小视差mind
    // 也即是左图中任何一点p，在右图上的匹配点的范围为应该是[p - maxd, p - mind], 而不需要遍历每一行所有的像素
    // maxd = baseline * length_focal / minZ
    // mind = baseline * length_focal / maxZ

    const float minZ = mb;
    const float minD = 0;			 
    const float maxD = mbf/minZ; 

    // 保存sad块匹配相似度和左图特征点索引
    vector<pair<int, int> > vDistIdx;
    vDistIdx.reserve(N);

    // 为左图每一个特征点il，在右图搜索最相似的特征点ir
    for(int iL=0; iL<N; iL++) {

		const cv::KeyPoint &kpL = mvKeys[iL];
        const int &levelL = kpL.octave;
        const float &vL = kpL.pt.y;
        const float &uL = kpL.pt.x;

        // 获取左图特征点il所在行，以及在右图对应行中可能的匹配点
        const vector<size_t> &vCandidates = vRowIndices[vL];
        if(vCandidates.empty()) continue;

        // 计算理论上的最佳搜索范围
        const float minU = uL-maxD;
        const float maxU = uL-minD;
        
        // 最大搜索范围小于0，说明无匹配点
        if(maxU<0) continue;

		// 初始化最佳相似度，用最大相似度，以及最佳匹配点索引
        int bestDist = ORBmatcher::TH_HIGH;
        size_t bestIdxR = 0;
        const cv::Mat &dL = mDescriptors.row(iL);
        
        // Step2. 粗配准. 左图特征点il与右图中的可能的匹配点进行逐个比较,得到最相似匹配点的相似度和索引
        for(size_t iC=0; iC<vCandidates.size(); iC++) {

            const size_t iR = vCandidates[iC];
            const cv::KeyPoint &kpR = mvKeysRight[iR];

            // 左图特征点il与带匹配点ic的空间尺度差超过2，放弃
            if(kpR.octave<levelL-1 || kpR.octave>levelL+1)
                continue;

            // 使用列坐标(x)进行匹配，和stereomatch一样
            const float &uR = kpR.pt.x;

            // 超出理论搜索范围[minU, maxU]，可能是误匹配，放弃
            if(uR >= minU && uR <= maxU) {

                // 计算匹配点il和待匹配点ic的相似度dist
                const cv::Mat &dR = mDescriptorsRight.row(iR);
                const int dist = ORBmatcher::DescriptorDistance(dL,dR);

				//统计最小相似度及其对应的列坐标(x)
                if( dist<bestDist ) {
                    bestDist = dist;
                    bestIdxR = iR;
                }
            }
        }
        
        // 如果刚才匹配过程中的最佳描述子距离小于给定的阈值
        // Step 3. 精确匹配. 
        if(bestDist<thOrbDist) {
            // 计算右图特征点x坐标和对应的金字塔尺度
            const float uR0 = mvKeysRight[bestIdxR].pt.x;
            const float scaleFactor = mvInvScaleFactors[kpL.octave];
            
            // 尺度缩放后的左右图特征点坐标
            const float scaleduL = round(kpL.pt.x*scaleFactor);			
            const float scaledvL = round(kpL.pt.y*scaleFactor);
            const float scaleduR0 = round(uR0*scaleFactor);

            // 滑动窗口搜索, 类似模版卷积或滤波
            // w表示sad相似度的窗口半径
            const int w = 5;

            // 提取左图中，以特征点(scaleduL,scaledvL)为中心, 半径为w的图像快patch
            cv::Mat IL = mpORBextractorLeft->mvImagePyramid[kpL.octave].rowRange(scaledvL-w,scaledvL+w+1).colRange(scaleduL-w,scaleduL+w+1);
            IL.convertTo(IL,CV_32F);
            
            // 图像块均值归一化，降低亮度变化对相似度计算的影响
			IL = IL - IL.at<float>(w,w) * cv::Mat::ones(IL.rows,IL.cols,CV_32F);

			//初始化最佳相似度
            int bestDist = INT_MAX;

			// 通过滑动窗口搜索优化，得到的列坐标偏移量
            int bestincR = 0;

			//滑动窗口的滑动范围为（-L, L）
            const int L = 5;

			// 初始化存储图像块相似度
            vector<float> vDists;
            vDists.resize(2*L+1); 

            // 计算滑动窗口滑动范围的边界，因为是块匹配，还要算上图像块的尺寸
            // 列方向起点 iniu = r0 + 最大窗口滑动范围 - 图像块尺寸
            // 列方向终点 eniu = r0 + 最大窗口滑动范围 + 图像块尺寸 + 1
            // 此次 + 1 和下面的提取图像块是列坐标+1是一样的，保证提取的图像块的宽是2 * w + 1
            const float iniu = scaleduR0+L-w;
            const float endu = scaleduR0+L+w+1;

			// 判断搜索是否越界
            if(iniu<0 || endu >= mpORBextractorRight->mvImagePyramid[kpL.octave].cols)
                continue;

			// 在搜索范围内从左到右滑动，并计算图像块相似度
            for(int incR=-L; incR<=+L; incR++) {

                // 提取左图中，以特征点(scaleduL,scaledvL)为中心, 半径为w的图像快patch
                cv::Mat IR = mpORBextractorRight->mvImagePyramid[kpL.octave].rowRange(scaledvL-w,scaledvL+w+1).colRange(scaleduR0+incR-w,scaleduR0+incR+w+1);
                IR.convertTo(IR,CV_32F);
                
                // 图像块均值归一化，降低亮度变化对相似度计算的影响
                IR = IR - IR.at<float>(w,w) * cv::Mat::ones(IR.rows,IR.cols,CV_32F);
                
                // sad 计算
                float dist = cv::norm(IL,IR,cv::NORM_L1);

                // 统计最小sad和偏移量
                if(dist<bestDist) {
                    bestDist = dist;
                    bestincR = incR;
                }

                //L+incR 为refine后的匹配点列坐标(x)
                vDists[L+incR] = dist; 	
            }

            // 搜索窗口越界判断ß 
            if(bestincR==-L || bestincR==L)
                continue;

			// Step 4. 亚像素插值, 使用最佳匹配点及其左右相邻点构成抛物线
            // 使用3点拟合抛物线的方式，用极小值代替之前计算的最优是差值
            //    \                 / <- 由视差为14，15，16的相似度拟合的抛物线
            //      .             .(16)
            //         .14     .(15) <- int/uchar最佳视差值
            //              . 
            //           （14.5）<- 真实的视差值
            //   deltaR = 15.5 - 16 = -0.5
            // 公式参考opencv sgbm源码中的亚像素插值公式
            // 或论文<<On Building an Accurate Stereo Matching System on Graphics Hardware>> 公式7

            const float dist1 = vDists[L+bestincR-1];	
            const float dist2 = vDists[L+bestincR];
            const float dist3 = vDists[L+bestincR+1];
            const float deltaR = (dist1-dist3)/(2.0f*(dist1+dist3-2.0f*dist2));

            // 亚像素精度的修正量应该是在[-1,1]之间，否则就是误匹配
            if(deltaR<-1 || deltaR>1)
                continue;
            
            // 根据亚像素精度偏移量delta调整最佳匹配索引
            float bestuR = mvScaleFactors[kpL.octave]*((float)scaleduR0+(float)bestincR+deltaR);
            float disparity = (uL-bestuR);
            if(disparity>=minD && disparity<maxD) {
                // 如果存在负视差，则约束为0.01
                if( disparity <=0 ) {
                    disparity=0.01;
                    bestuR = uL-0.01;
                }
                
                // 根据视差值计算深度信息
                // 保存最相似点的列坐标(x)信息
                // 保存归一化sad最小相似度
                // Step 5. 最优视差值/深度选择.
                mvDepth[iL]=mbf/disparity;
                mvuRight[iL] = bestuR;
                vDistIdx.push_back(pair<int,int>(bestDist,iL));
        }   
    }

    // Step 6. 删除离缺点(outliers)
    // 块匹配相似度阈值判断，归一化sad最小，并不代表就一定是匹配的，比如光照变化、弱纹理、无纹理等同样会造成误匹配
    // 误匹配判断条件  norm_sad > 1.5 * 1.4 * median
    sort(vDistIdx.begin(),vDistIdx.end());
    const float median = vDistIdx[vDistIdx.size()/2].first;
    const float thDist = 1.5f*1.4f*median;

    for(int i=vDistIdx.size()-1;i>=0;i--) {
        if(vDistIdx[i].first<thDist)
            break;
        else {
			// 误匹配点置为-1，和初始化时保持一直，作为error code
            mvuRight[vDistIdx[i].second]=-1;
            mvDepth[vDistIdx[i].second]=-1;
        }
    }
}

//计算RGBD图像的立体深度信息
void Frame::ComputeStereoFromRGBD(const cv::Mat &imDepth)	//参数是深度图像
{
    /** 主要步骤如下:.对于彩色图像中的每一个特征点:<ul>  */
    // mvDepth直接由depth图像读取`
	//这里是初始化这两个存储“右图”匹配特征点横坐标和存储特征点深度值的vector
    mvuRight = vector<float>(N,-1);
    mvDepth = vector<float>(N,-1);

	//开始遍历彩色图像中的所有特征点
    for(int i=0; i<N; i++)
    {
        /** <li> 从<b>未矫正的特征点</b>提供的坐标来读取深度图像拿到这个点的深度数据 </li> */
		//获取校正前和校正后的特征点
        const cv::KeyPoint &kp = mvKeys[i];
        const cv::KeyPoint &kpU = mvKeysUn[i];

		//获取其横纵坐标，注意 NOTICE 是校正前的特征点的
        const float &v = kp.pt.y;
        const float &u = kp.pt.x;
		//从深度图像中获取这个特征点对应的深度点
        //NOTE 从这里看对深度图像进行去畸变处理是没有必要的,我们依旧可以直接通过未矫正的特征点的坐标来直接拿到深度数据
        const float d = imDepth.at<float>(v,u);

		//
        /** <li> 如果获取到的深度点合法(d>0), 那么就保存这个特征点的深度,并且计算出等效的\在假想的右图中该特征点所匹配的特征点的横坐标 </li>
         * \n 这个横坐标的计算是 x-mbf/d
         * \n 其中的x使用的是<b>矫正后的</b>特征点的图像坐标
         */
        if(d>0)
        {
			//那么就保存这个点的深度
            mvDepth[i] = d;
			//根据这个点的深度计算出等效的、在假想的右图中的该特征点的横坐标
			//TODO 话说为什么要计算这个嘞，计算出来之后有什么用?可能是为了保持计算一致
            mvuRight[i] = kpU.pt.x-mbf/d;
        }//如果获取到的深度点合法
    }//开始遍历彩色图像中的所有特征点
    /** </ul> */
}

//当某个特征点的深度信息或者双目信息有效时,将它反投影到三维世界坐标系中
cv::Mat Frame::UnprojectStereo(const int &i)
{
    // KeyFrame::UnprojectStereo 
	// 貌似这里普通帧的反投影函数操作过程和关键帧的反投影函数操作过程有一些不同：
    // mvDepth是在ComputeStereoMatches函数中求取的
	// TODO 验证下面的这些内容. 虽然现在我感觉是理解错了,但是不确定;如果确定是真的理解错了,那么就删除下面的内容
    // mvDepth对应的校正前的特征点，可这里却是对校正后特征点反投影
    // KeyFrame::UnprojectStereo中是对校正前的特征点mvKeys反投影
    // 在ComputeStereoMatches函数中应该对校正后的特征点求深度？？ (wubo???)
	// NOTE 不过我记得好像上面的ComputeStereoMatches函数就是对于双目相机设计的，而双目相机的图像默认都是经过了校正的啊

    /** 步骤如下: <ul> */

    /** <li> 获取这个特征点的深度（这里的深度可能是通过双目视差得出的，也可能是直接通过深度图像的出来的） </li> */
	const float z = mvDepth[i];
    /** <li> 判断这个深度是否合法 </li> <ul> */
	//（其实这里也可以不再进行判断，因为在计算或者生成这个深度的时候都是经过检查了的_不行,RGBD的不是）
    if(z>0)
    {
        /** <li> 如果合法,就利用<b></b>矫正后的特征点的坐标 Frame::mvKeysUn 和相机的内参数,通过反投影和位姿变换得到空间点的坐标 </li> */
		//获取像素坐标，注意这里是矫正后的特征点的坐标
        const float u = mvKeysUn[i].pt.x;
        const float v = mvKeysUn[i].pt.y;
		//计算在当前相机坐标系下的坐标
        const float x = (u-cx)*z*invfx;
        const float y = (v-cy)*z*invfy;
		//生成三维点（在当前相机坐标系下）
        cv::Mat x3Dc = (cv::Mat_<float>(3,1) << x, y, z);
		//然后计算这个点在世界坐标系下的坐标，这里是对的，但是公式还是要斟酌一下。首先变换成在没有旋转的相机坐标系下，最后考虑相机坐标系相对于世界坐标系的平移
        return mRwc*x3Dc+mOw;
    }
    else
        /** <li> 如果深度值不合法，那么就返回一个空矩阵,表示计算失败 </li> */
        return cv::Mat();
    /** </ul> */
    /** </ul> */
}

} //namespace ORB_SLAM
