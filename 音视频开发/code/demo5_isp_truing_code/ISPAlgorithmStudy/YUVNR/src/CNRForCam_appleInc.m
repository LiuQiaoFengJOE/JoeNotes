%% --------------------------------
%% author:wtzhu
%% date: 20230507
%% fuction: 
%% note: Chroma noise reduction for cameras -- Apple Inc.
%%       暂不考虑存储问题，HV同步卷积
%% --------------------------------
clc;clear;close all;
%% =============== global values ============= 
th = 150;
kernelSize = [25, 25];

%% =============== data prepare =============
img_rgb = imread('./images/UVNR_off.png');
img_ycbcr = rgb2ycbcr(img_rgb);
[h, w, c] = size(img_rgb);

figure();
subplot(131), imshow(uint8(img_ycbcr(:,:,1))), title('Y');
subplot(132), imshow(uint8(img_ycbcr(:,:,2))), title('Cb');
subplot(133), imshow(uint8(img_ycbcr(:,:,3))), title('Cr');

padSize = floor(kernelSize / 2);
img_ycbcr_pad = double(padarray(img_ycbcr, padSize,'replicate','both'));
T = zeros(kernelSize);
des_img = zeros(h, w, c);
%% =============== main loop ============= 
for i = 1: h
    for j = 1: w
        roi = img_ycbcr_pad(i: i+kernelSize(1)-1, j: j+kernelSize(2)-1, :);
        
        % ----------- loop for the roi(Conv) ---------
        % todo: 将内部循环改为矩阵运算，加快速度
        desCr = 0;
        desCb = 0;
        for ii = 1: kernelSize(1)
            for jj = 1: kernelSize(2)
                cPos = round(kernelSize / 2);
                delta_cb = abs(roi(ii, jj, 2) - roi(cPos(1), cPos(2), 2));
                delta_cr = abs(roi(ii, jj, 3) - roi(cPos(1), cPos(2), 3));
                delta = delta_cb + delta_cr;
                if delta >  th
                    T((ii-1)*kernelSize(1) + jj) = 0;
                else
                    T((ii-1)*kernelSize(1) + jj) = 1;
                end
                desCb = roi(ii, jj, 2) * T((ii-1)*kernelSize(1) + jj) + desCb;
                desCr = roi(ii, jj, 3) * T((ii-1)*kernelSize(1) + jj) + desCr;
            end
        end
        desCb = desCb / sum(sum(T));
        desCr = desCr / sum(sum(T));
        des_img(i, j, 1) = img_ycbcr(i, j, 1);
        des_img(i, j, 2) = desCb;
        des_img(i, j, 3) = desCr;
    end
end
% use bilter filter on Y
des_img(:, :, 1) = imbilatfilt(des_img(:, :, 1));

%% =============== show res ============= 
figure()
subplot(131), imshow(uint8(des_img(:,:,1))), title('desY');
subplot(132), imshow(uint8(des_img(:,:,2))), title('desCb');
subplot(133), imshow(uint8(des_img(:,:,3))), title('desCr');

des_img_rgb = ycbcr2rgb(uint8(des_img));
imwrite(des_img_rgb, 'images/UVNR_off-1.png');
figure(), 
subplot(121), imshow(img_rgb), title('org');
subplot(122), imshow(des_img_rgb), title('NR');


