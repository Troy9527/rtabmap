/*
Copyright (c) 2010-2016, Mathieu Labbe - IntRoLab - Universite de Sherbrooke
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Universite de Sherbrooke nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <rtabmap/core/stereo/StereoBM.h>
#include <rtabmap/utilite/ULogger.h>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>
#include <iostream>
#include <fstream>

/* Socket */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <string>
#include <cstdio>
#include <opencv2/opencv.hpp>

#define SOCK_PATH "/tmp/psmnet.sock"

namespace rtabmap {

StereoBM::StereoBM(int blockSize, int numDisparities) :
		blockSize_(blockSize),
		minDisparity_(Parameters::defaultStereoBMMinDisparity()),
		numDisparities_(numDisparities),
		preFilterSize_(Parameters::defaultStereoBMPreFilterSize()),
		preFilterCap_(Parameters::defaultStereoBMPreFilterCap()),
		uniquenessRatio_(Parameters::defaultStereoBMUniquenessRatio()),
		textureThreshold_(Parameters::defaultStereoBMTextureThreshold()),
		speckleWindowSize_(Parameters::defaultStereoBMSpeckleWindowSize()),
		speckleRange_(Parameters::defaultStereoBMSpeckleRange()),
		disp12MaxDiff_(Parameters::defaultStereoBMDisp12MaxDiff())
{
}
StereoBM::StereoBM(const ParametersMap & parameters) :
		StereoDense(parameters),
		blockSize_(Parameters::defaultStereoBMBlockSize()),
		minDisparity_(Parameters::defaultStereoBMMinDisparity()),
		numDisparities_(Parameters::defaultStereoBMNumDisparities()),
		preFilterSize_(Parameters::defaultStereoBMPreFilterSize()),
		preFilterCap_(Parameters::defaultStereoBMPreFilterCap()),
		uniquenessRatio_(Parameters::defaultStereoBMUniquenessRatio()),
		textureThreshold_(Parameters::defaultStereoBMTextureThreshold()),
		speckleWindowSize_(Parameters::defaultStereoBMSpeckleWindowSize()),
		speckleRange_(Parameters::defaultStereoBMSpeckleRange()),
		disp12MaxDiff_(Parameters::defaultStereoBMDisp12MaxDiff())
{
	this->parseParameters(parameters);
}

void StereoBM::parseParameters(const ParametersMap & parameters)
{
	Parameters::parse(parameters, Parameters::kStereoBMBlockSize(), blockSize_);
	Parameters::parse(parameters, Parameters::kStereoBMMinDisparity(), minDisparity_);
	Parameters::parse(parameters, Parameters::kStereoBMNumDisparities(), numDisparities_);
	Parameters::parse(parameters, Parameters::kStereoBMPreFilterSize(), preFilterSize_);
	Parameters::parse(parameters, Parameters::kStereoBMPreFilterCap(), preFilterCap_);
	Parameters::parse(parameters, Parameters::kStereoBMUniquenessRatio(), uniquenessRatio_);
	Parameters::parse(parameters, Parameters::kStereoBMTextureThreshold(), textureThreshold_);
	Parameters::parse(parameters, Parameters::kStereoBMSpeckleWindowSize(), speckleWindowSize_);
	Parameters::parse(parameters, Parameters::kStereoBMSpeckleRange(), speckleRange_);
	Parameters::parse(parameters, Parameters::kStereoBMDisp12MaxDiff(), disp12MaxDiff_);
}

cv::Mat StereoBM::computeDisparity(
		const cv::Mat & leftImage,
		const cv::Mat & rightImage) const
{
	UASSERT(!leftImage.empty() && !rightImage.empty());
	UASSERT(leftImage.cols == rightImage.cols && leftImage.rows == rightImage.rows);
	UASSERT((leftImage.type() == CV_8UC1 || leftImage.type() == CV_8UC3) && rightImage.type() == CV_8UC1);

	cv::Mat leftMono;
		/*std::cout << leftImage.cols << " " << leftImage.rows << " " << leftImage.channels() << std::endl;*/
	if(leftImage.channels() == 3)
	{
		cv::cvtColor(leftImage, leftMono, CV_BGR2GRAY);
		std::cout << "gray" << std::endl;
	}
	else
	{
		leftMono = leftImage;
	}

	cv::Mat disparity;
/*#if CV_MAJOR_VERSION < 3
	cv::StereoBM stereo(cv::StereoBM::BASIC_PRESET);
	stereo.state->SADWindowSize = blockSize_;
	stereo.state->minDisparity = minDisparity_;
	stereo.state->numberOfDisparities = numDisparities_;
	stereo.state->preFilterSize = preFilterSize_;
	stereo.state->preFilterCap = preFilterCap_;
	stereo.state->uniquenessRatio = uniquenessRatio_;
	stereo.state->textureThreshold = textureThreshold_;
	stereo.state->speckleWindowSize = speckleWindowSize_;
	stereo.state->speckleRange = speckleRange_;
	stereo(leftMono, rightImage, disparity, CV_16SC1);
#else
	cv::Ptr<cv::StereoBM> stereo = cv::StereoBM::create();
	stereo->setBlockSize(blockSize_);
	stereo->setMinDisparity(minDisparity_);
	stereo->setNumDisparities(numDisparities_);
	stereo->setPreFilterSize(preFilterSize_);
	stereo->setPreFilterCap(preFilterCap_);
	stereo->setUniquenessRatio(uniquenessRatio_);
	stereo->setTextureThreshold(textureThreshold_);
	stereo->setSpeckleWindowSize(speckleWindowSize_);
	stereo->setSpeckleRange(speckleRange_);
	stereo->setDisp12MaxDiff(disp12MaxDiff_);
	stereo->compute(leftMono, rightImage, disparity);
#endif*/
	/*std::cout << "BM " << _count << std::endl; */

	int sockfd, disp_bytes, total_bytes = 0, recv_bytes;
	struct sockaddr_un dest;
	char buffer[16], buffer2[10*1024];

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	bzero(&dest, sizeof(dest));
	dest.sun_family = AF_UNIX;
	strcpy(dest.sun_path, SOCK_PATH);
	
	connect(sockfd, (struct sockaddr*)&dest, sizeof(dest));

	 /*send left image */
	std::vector<unsigned char> left_encode;
	cv::imencode(".png", leftImage, left_encode);

	memset(buffer, 0, sizeof(buffer));
	std::sprintf(buffer, "%d", (int)left_encode.size());
	send(sockfd, buffer, sizeof(buffer), 0);
	recv(sockfd, buffer, sizeof(buffer), 0);
	send(sockfd, &left_encode[0], left_encode.size(), 0);

	recv(sockfd, buffer, sizeof(buffer), 0);
	
	 /*send right image */
	std::vector<unsigned char> right_encode;
	cv::imencode(".png", rightImage, right_encode);
	
	memset(buffer, 0, sizeof(buffer));
	std::sprintf(buffer, "%d", (int)right_encode.size());
	send(sockfd, buffer, sizeof(buffer), 0);
	recv(sockfd, buffer, sizeof(buffer), 0);
	send(sockfd, &right_encode[0], right_encode.size(), 0);

	 /*receive disparity map */
	memset(buffer, 0, sizeof(buffer));
	recv(sockfd, buffer, sizeof(buffer), 0);
	
	sscanf(buffer, "%d", &disp_bytes);

	send(sockfd, buffer, sizeof(buffer), 0);
	
	std::vector<unsigned char> disp;
	while(total_bytes < disp_bytes){
		memset(buffer2, 0, sizeof(buffer2));
		recv_bytes = recv(sockfd, buffer2, sizeof(buffer2), 0);
		total_bytes += recv_bytes;
		disp.insert(disp.end(), buffer2, buffer2 + recv_bytes);
	}
	
	cv::Mat tmp = cv::imdecode(disp, CV_LOAD_IMAGE_GRAYSCALE);
	tmp.convertTo(tmp, CV_32FC1);
	close(sockfd);
	/*std::string path = "/home/troy/project/test/disp", end = ".png", num = std::to_string(_count);*/
	/*cv::imwrite(path+num+end, tmp);*/
	/*cv::imwrite(path+"2_"+num+end, disparity);*/

	/*return disparity;*/
	return tmp;
}

} /* namespace rtabmap */
