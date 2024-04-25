#include "ofApp.h"
#include "ply.h"
#include <cstdint> // For uint32_t and uint16_t
#include <cstring> // For memcpy


void convertMatrixTo1DArray(const ofMatrix4x4& matrix, float array[16]) {
    int index = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            // Assuming the internal storage is _mat[row][col],
            // adjust if the actual storage differs
            array[index++] = matrix._mat[row][col];
        }
    }
}

void convertMatrixTo1DArray2(const ofMatrix4x4& matrix, float array[16]) {
    int index = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            // Assuming the internal storage is _mat[row][col],
            // adjust if the actual storage differs
            array[index++] = matrix._mat[col][row];
        }
    }
}

struct VertexData {
    float x, y, z;
    float opacity;
    float scale[3];
    float rot[4];
    float f_dc[3];
};



void sort_fast(vector < VertexData > buf,  glm::mat4 & P, ofApp::SortResult* out) {
    // From https://github.com/antimatter15/splat
    const size_t N = buf.size();
    
    
    if (N==0)
        return;
    
    float a[16];
    convertMatrixTo1DArray(P,a);
    
    float min_d = std::numeric_limits<float>::infinity();
    float max_d = -std::numeric_limits<float>::infinity();

    {
        constexpr int DEPTH_SCALE = 4096;
        for (size_t i = 0; i < N; ++i) {
            /*           const float depth = DEPTH_SCALE * P.block<1, 3>(2, 0).dot(
                           ofVec3(buf.at(i).center[0],
                                           buf.at(i).center[1],
                                           buf.at(i).center[2]));*/

//            const float depth = (
//               (P.getRowAsVec3f(2)[1] * buf.at(i).x +
//                P.getRowAsVec3f(2)[2] * buf.at(i).y +
//                P.getRowAsVec4f(2)[3] * buf.at(i).z))*
//                DEPTH_SCALE;
                 float depth = (
                   (a[2] * buf.at(i).x +
                    a[6] * buf.at(i).y +
                    a[10] * buf.at(i).z))*
                    DEPTH_SCALE;
            
            glm::vec4 transformedPosition = P * glm::vec4(buf.at(i).x,
                                                                   buf.at(i).y,
                                                                   buf.at(i).z, 1.0);
            depth = glm::length(glm::vec3(transformedPosition));
            
            out->sizes[i] = static_cast<int>(depth);
            max_d = std::max(depth, max_d);
            min_d = std::min(depth, min_d);
        }
    }

    {
        //tracing::RecorderGuard tracing_guard("counting sort");
        const int32_t M = static_cast<int32_t>(out->counts0.size());
        const float depth_inv = M / (max_d - min_d);
        for (size_t i = 0; i < N; ++i) {
            out->sizes[i] =
                ofClamp(static_cast<int32_t>((out->sizes[i] - min_d) * depth_inv), 0, M - 1);
            ++(out->counts0[out->sizes[i]]);
        }
        for (size_t i = 1; i < static_cast<size_t>(M); ++i)
            out->starts0[i] = out->starts0[i - 1] + out->counts0[i - 1];
        for (size_t i = 0; i < N; ++i){
            int sz = out->starts0[out->sizes[i]]++;
           // cout << i << " " << sz << " " << endl;
            if(sz>=0 && sz < N*6.){
                for(int s = 0; s < 6 ; s++){
                    out->depth_index[sz*6+s] = (i*6.) +s;//
                }
                //ofLog(OF_LOG_NOTICE, "assign");
                //cout<<"assign";
            }
        }
    }
}



// Utility function to convert quaternion to rotation matrix
glm::mat3 quaternionToMatrix(const glm::quat& q) {
    // Construct a 3x3 rotation matrix from the quaternion
    glm::mat3 matrix;
    float qx, qy, qz, qw;
    qx = q.x;
    qy = q.y;
    qz = q.z;
    qw = q.w;

    matrix[0][0] = 1 - 2 * qy * qy - 2 * qz * qz;
    matrix[0][1] = 2 * qx * qy - 2 * qz * qw;
    matrix[0][2] = 2 * qx * qz + 2 * qy * qw;

    matrix[1][0] = 2 * qx * qy + 2 * qz * qw;
    matrix[1][1] = 1 - 2 * qx * qx - 2 * qz * qz;
    matrix[1][2] = 2 * qy * qz - 2 * qx * qw;

    matrix[2][0] = 2 * qx * qz - 2 * qy * qw;
    matrix[2][1] = 2 * qy * qz + 2 * qx * qw;
    matrix[2][2] = 1 - 2 * qx * qx - 2 * qy * qy;

    return matrix;
}

glm::mat3 scaleRotationMatrix(const glm::mat3& rotationMatrix, const glm::vec3& scale) {
    glm::mat3 scaledMatrix;
    scaledMatrix[0] = rotationMatrix[0] * scale.x; // Scale x-axis
    scaledMatrix[1] = rotationMatrix[1] * scale.y; // Scale y-axis
    scaledMatrix[2] = rotationMatrix[2] * scale.z; // Scale z-axis
    return scaledMatrix;
}




vector < VertexData > vertices;


//--------------------------------------------------------------
void ofApp::setup(){
    shader.load("vert.glsl", "frag.glsl");
    shader.bindDefaults();
    const std::string& filename = ofToDataPath("point_cloud.ply");

    
     ply::PlyFile ply(filename);

     // Create accessors
     const auto x = ply.accessor<float>("x");
     const auto y = ply.accessor<float>("y");
     const auto z = ply.accessor<float>("z");
     const auto opacity = ply.accessor<float>("opacity");
     const auto scale_0 = ply.accessor<float>("scale_0");
     const auto scale_1 = ply.accessor<float>("scale_1");
     const auto scale_2 = ply.accessor<float>("scale_2");
     const auto rot_qw = ply.accessor<float>("rot_0");
     const auto rot_qx = ply.accessor<float>("rot_1");
     const auto rot_qy = ply.accessor<float>("rot_2");
     const auto rot_qz = ply.accessor<float>("rot_3");
     // Spherical harmonics accessors
     const auto f_dc_0 = ply.accessor<float>("f_dc_0");
     const auto f_dc_1 = ply.accessor<float>("f_dc_1");
     const auto f_dc_2 = ply.accessor<float>("f_dc_2");
    
     std::vector<ply::PlyAccessor<float>> sh;
     for (size_t i = 0; i < 45; ++i)
         sh.push_back(ply.accessor<float>("f_rest_" + std::to_string(i)));

    for (size_t row = 0; row < ply.num_vertices(); ++row) {
        
        VertexData temp;
        temp.x = x(row);
        temp.y = y(row);
        temp.z = z(row);
        temp.opacity = 1.f / (1.f + std::exp(-opacity(row)));
        temp.scale[0] = exp(scale_0(row));
        temp.scale[1] = exp(scale_1(row));
        temp.scale[2] = exp(scale_2(row));
        
        float qlen = sqrt( pow(rot_qx(row), 2) +
                           pow(rot_qy(row), 2) +
                           pow(rot_qz(row), 2) +
                           pow(rot_qw(row), 2));
        
        temp.rot[0] = (rot_qx(row)/qlen);
        temp.rot[1] = (rot_qy(row)/qlen);
        temp.rot[2] = (rot_qz(row)/qlen);
        temp.rot[3] = (rot_qw(row)/qlen);
        temp.f_dc[0] = f_dc_0(row);
        temp.f_dc[1] = f_dc_1(row);
        temp.f_dc[2] = f_dc_2(row);
        
        
        vertices.push_back(temp);

    }
    

    
    size_t vertexCount = vertices.size();
    
    
    // subtract the center
    ofPoint center;
    for (int i = 0; i < vertexCount; i++) {
        center +=ofPoint(vertices[i].x, vertices[i].y, vertices[i].z);
    }
    center/= (float)vertexCount;
    ofMatrix4x4 rot;
    //rot = ofMatrix4x4::makeRotationMatrix(PI, 0, 1, 0);
    for (int i = 0; i < vertexCount; i++) {
        vertices[i].x -= center.x;
        vertices[i].y -= center.y;
       // vertices[i].y*=-1;
        //vertices[i].x*=-1;
        
        vertices[i].z -= center.z;
        //vertices[i].z *= -1;
//        vertices[i].x *= 100.;
//        vertices[i].y *= 100.;
//        vertices[i].z *= 100.;
        
        
    }

  
    
    
    vector < float > data;
    
    for (size_t i = 0; i < vertexCount; ++i) {
        
        
        
            const auto& v = vertices[i];

            float SH_C0 = 0.28209479177387814;
            float r = 0.5 + SH_C0 * v.f_dc[0];
            float g = 0.5 + SH_C0 * v.f_dc[1];
            float b = 0.5 + SH_C0 * v.f_dc[2];
            float a = 1 / (1 + exp(-v.opacity)); // Opacity converted to alpha

            // Convert RGBA to 0-255 range and store
            uint32_t color = ((uint8_t)(r * 255) << 24) | ((uint8_t)(g * 255) << 16) | ((uint8_t)(b * 255) << 8) | (uint8_t)(a * 255);
//
//            glm::quat quaternion(v.rot[0], v.rot[1], v.rot[2], v.rot[3]);
//               glm::mat3 rotationMatrix = quaternionToMatrix(quaternion);
//
//               // Apply scale to the rotation matrix
//
//        glm::mat3 scaledRotationMatrix = scaleRotationMatrix(rotationMatrix, glm::vec3(v.scale[0], v.scale[1], v.scale[2]));
//
//
//               // Compute sigma values from the scaled rotation matrix
//               float sigma[6];
//               sigma[0] = scaledRotationMatrix[0][0] * scaledRotationMatrix[0][0] + scaledRotationMatrix[1][0] * scaledRotationMatrix[1][0] + scaledRotationMatrix[2][0] * scaledRotationMatrix[2][0];
//               sigma[1] = scaledRotationMatrix[0][0] * scaledRotationMatrix[0][1] + scaledRotationMatrix[1][0] * scaledRotationMatrix[1][1] + scaledRotationMatrix[2][0] * scaledRotationMatrix[2][1];
//               sigma[2] = scaledRotationMatrix[0][0] * scaledRotationMatrix[0][2] + scaledRotationMatrix[1][0] * scaledRotationMatrix[1][2] + scaledRotationMatrix[2][0] * scaledRotationMatrix[2][2];
//               sigma[3] = scaledRotationMatrix[0][1] * scaledRotationMatrix[0][1] + scaledRotationMatrix[1][1] * scaledRotationMatrix[1][1] + scaledRotationMatrix[2][1] * scaledRotationMatrix[2][1];
//               sigma[4] = scaledRotationMatrix[0][1] * scaledRotationMatrix[0][2] + scaledRotationMatrix[1][1] * scaledRotationMatrix[1][2] + scaledRotationMatrix[2][1] * scaledRotationMatrix[2][2];
//               sigma[5] = scaledRotationMatrix[0][2] * scaledRotationMatrix[0][2] + scaledRotationMatrix[1][2] * scaledRotationMatrix[1][2] + scaledRotationMatrix[2][2] * scaledRotationMatrix[2][2];

        
        std::vector<double> rot = { v.rot[0], v.rot[1], v.rot[2], v.rot[3] };
        std::vector<double> scale = { v.scale[0], v.scale[1], v.scale[2] };
        std::vector<double> M(9, 0.0); // 9 elements for a 3x3 matrix
           M[0] = 1.0 - 2.0 * (rot[2] * rot[2] + rot[3] * rot[3]);
           M[1] = 2.0 * (rot[1] * rot[2] + rot[0] * rot[3]);
           M[2] = 2.0 * (rot[1] * rot[3] - rot[0] * rot[2]);
           M[3] = 2.0 * (rot[1] * rot[2] - rot[0] * rot[3]);
           M[4] = 1.0 - 2.0 * (rot[1] * rot[1] + rot[3] * rot[3]);
           M[5] = 2.0 * (rot[2] * rot[3] + rot[0] * rot[1]);
           M[6] = 2.0 * (rot[1] * rot[3] + rot[0] * rot[2]);
           M[7] = 2.0 * (rot[2] * rot[3] - rot[0] * rot[1]);
           M[8] = 1.0 - 2.0 * (rot[1] * rot[1] + rot[2] * rot[2]);

           // Apply scaling
           for (int i = 0; i < 9; ++i) {
               M[i] *= scale[i / 3];
           }

           // Calculate sigma
           std::vector<float> sigma(6, 0.0);
           sigma[0] = M[0] * M[0] + M[3] * M[3] + M[6] * M[6];
           sigma[1] = M[0] * M[1] + M[3] * M[4] + M[6] * M[7];
           sigma[2] = M[0] * M[2] + M[3] * M[5] + M[6] * M[8];
           sigma[3] = M[1] * M[1] + M[4] * M[4] + M[7] * M[7];
           sigma[4] = M[1] * M[2] + M[4] * M[5] + M[7] * M[8];
           sigma[5] = M[2] * M[2] + M[5] * M[5] + M[8] * M[8];

        
        
        std::vector<float> customData = {
            v.x, v.y, v.z, r, g, b, a, 4*sigma[0],
            4*sigma[1], 4*sigma[2], 4*sigma[3], 4*sigma[4], 4*sigma[5]};
        
        for (int z = 0; z < 6; z++){
            for (auto & f : customData){
                data.push_back(f);
            }
        }
    }

    mesh.setMode(OF_PRIMITIVE_TRIANGLES);
    
    vector < unsigned int > indices;
    for (int i = 0; i < vertexCount; i++) {
        
        mesh.addVertex(ofPoint(-2, -2));
        mesh.addVertex(ofPoint(2, -2));
        mesh.addVertex(ofPoint(2, 2));
        
        
        mesh.addVertex(ofPoint(-2, -2));
        mesh.addVertex(ofPoint(-2, 2));
        mesh.addVertex(ofPoint(2, 2));
       
        mesh.addColor(ofFloatColor( 1, 0, 1) );
        mesh.addColor(ofFloatColor( 1, 0, 1) );
        mesh.addColor(ofFloatColor( 1, 0, 1) );
        
        mesh.addColor(ofFloatColor( 1, 0, 1) );
        mesh.addColor(ofFloatColor( 1, 0, 1) );
        mesh.addColor(ofFloatColor( 1, 0, 1) );
        
        indices.push_back(i * 6 + 0);
        indices.push_back(i * 6 + 1);
        indices.push_back(i * 6 + 2);
        indices.push_back(i * 6 + 3);
        indices.push_back(i * 6 + 4);
        indices.push_back(i * 6 + 5);
        

    }
    
    mesh.addIndices(indices);
    
    
    shader.begin();
    int customData1Loc = shader.getAttributeLocation("customData1");
    int customData2Loc = shader.getAttributeLocation("customData2");
    int customData3Loc = shader.getAttributeLocation("customData3");
    int customData4Loc = shader.getAttributeLocation("customData4");

    mesh.getVbo().setAttributeData(customData1Loc, data.data(), 4, vertices.size()*6, GL_STATIC_DRAW, sizeof(float) * 13); // First vec4
    mesh.getVbo().setAttributeData(customData2Loc, data.data() + 4, 4, vertices.size()*6, GL_STATIC_DRAW, sizeof(float) * 13); // Second vec4
    mesh.getVbo().setAttributeData(customData3Loc, data.data() + 8, 3, vertices.size()*6, GL_STATIC_DRAW, sizeof(float) * 13); // First vec3
    mesh.getVbo().setAttributeData(customData4Loc, data.data() + 11, 2, vertices.size()*6, GL_STATIC_DRAW, sizeof(float) * 13); // Remaining vec2 (for the last 2 floats)
    
    

    
     shader.end();
    
  
    
    //enableIndices
    mesh.enableIndices();
    sr.reset(vertexCount*6.);
}

//--------------------------------------------------------------
void ofApp::update(){

    if (ofGetFrameNum() % 60 == 0){
        shader.load("vert.glsl", "frag.glsl");
        shader.bindDefaults();
    }
    

}

//--------------------------------------------------------------
void ofApp::draw(){
    cam.begin();
    ofRotateX(mouseX);
    //use shader program
    ofDisableDepthTest();
    //ofBackground(0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Clear
    
    glBlendFuncSeparate(
        GL_ONE_MINUS_DST_ALPHA,
        GL_ONE,
        GL_ONE_MINUS_DST_ALPHA,
        GL_ONE
    );
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
 
    shader.begin();
    
    
    //cam.resetTransform();
    
    glm::vec3 cameraPos = glm::vec3(cam.getPosition().x,
                                    cam.getPosition().y,
                                    cam.getPosition().z);    // Camera position in world coordinates
    
    auto target  = cam.getTarget();
    glm::vec3 cameraTarget = target.getPosition(); // Where the camera is looking
    glm::vec3 upVector = glm::vec3(0, 1, 0);    // Typically Y is up

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, upVector);
    
    
//    if (ofGetMousePressed()){
//        shader.setUniformMatrix4f("view",cam.getModelViewMatrix());
//    } else {
        shader.setUniformMatrix4f("view",cam.getModelViewMatrix());
    //}
    
   //cout << view << " " << cam.getModelViewMatrix() << endl;
    shader.setUniform2f("viewport", ofGetWidth(), ofGetHeight());
    shader.setUniformMatrix4f("projection", cam.getProjectionMatrix());
    shader.setUniform1f("time", ofGetElapsedTimef());
    float fov = cam.getFov();
    float viewportWidth = ofGetViewportWidth();
    float viewportHeight = ofGetViewportHeight();

    float focalLengthX = viewportWidth / (2.0f * tan(ofDegToRad(fov) / 2.0f));
    float focalLengthY = viewportHeight / (2.0f * tan(ofDegToRad(fov) / 2.0f));
    
    
    
    shader.setUniform2f("focal", focalLengthX ,focalLengthY);
    shader.setUniform3f("cam_pos", cam.getPosition().x, cam.getPosition().y, cam.getPosition().z);
    
    cam.begin();
    
    glm::mat4 viewMatrix = (cam.getModelViewMatrix());
      
    //sort_fast(vertices,viewMatrix,&sr);
    
    typedef struct {
        ofPoint pt;
        float distance;
        int index;
    } vertex2;
    vector < vertex2 > vertexsss;
    for (int i = 0; i < vertices.size(); i++){
        vertex2 v;
        v.pt = ofPoint(vertices[i].x,vertices[i].y,vertices[i].z);
        v.index = i;
        v.distance = cam.worldToScreen(v.pt).z;
        vertexsss.push_back(v);
    }
    std::sort(vertexsss.begin(), vertexsss.end(), [](const vertex2 &a, const vertex2 &b)
    {
        return a.distance < b.distance;
    });
    
    
    
    
    cam.end();
    //mesh.getVbo().setIndexData(sr.depth_index.data(),vertices.size()*6., GL_STATIC_DRAW);

   // mesh.getVbo().updateIndexData(<#const ofIndexType *indices#>, <#int total#>)
    
    //let's randomize the indices
    
    vector <  int > draworder;
    for (int i = 0; i < vertices.size(); i++) {
        draworder.push_back(i);
    }
    ofRandomize(draworder);
    vector < unsigned int > indices;
    for (int i = 0; i < vertexsss.size(); i++) {
        
        int index = vertexsss[i].index;
        indices.push_back(index*6 + 0);
        indices.push_back(index*6 + 1);
        indices.push_back(index*6 + 2);
        indices.push_back(index*6 + 3);
        indices.push_back(index*6 + 4);
        indices.push_back(index*6 + 5);

        //draworder.push_back(i);
    }
    
    mesh.getVbo().updateIndexData(indices.data(), indices.size());
    
    mesh.draw();
    shader.end();

    cam.end();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}
