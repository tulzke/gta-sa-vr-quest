#include "HandSkeleton.h"
#define CGLTF_IMPLEMENTATION
#include "../vendor/cgltf/cgltf.h"
#include <memory>
#include <cstdlib>

namespace savr::handskeleton {
Mat Identity() { Mat m{};m[0]=m[5]=m[10]=m[15]=1;return m; }
Mat Multiply(const Mat& a,const Mat& b) {
    Mat m{};
    for(int c=0;c<4;++c) for(int r=0;r<4;++r)
        for(int k=0;k<4;++k) m[c*4+r]+=a[k*4+r]*b[c*4+k];
    return m;
}
Mat Matrix(const Transform& t) {
    const auto& q=t.q; const float x=q[0],y=q[1],z=q[2],w=q[3];
    Mat m{1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
          2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
          2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,t.t[0],t.t[1],t.t[2],1};
    for(int c=0;c<3;++c) for(int r=0;r<3;++r) m[c*4+r]*=t.s[c];
    return m;
}
Quat Slerp(Quat a,Quat b,float t) {
    float dot=0; for(int i=0;i<4;++i) dot+=a[i]*b[i];
    if(dot<0) {dot=-dot;for(auto& x:b)x=-x;}
    dot=std::clamp(dot,0.f,1.f);
    float wa=1-t,wb=t;
    if(dot<.9995f) {const float angle=std::acos(dot),s=std::sin(angle);wa=std::sin((1-t)*angle)/s;wb=std::sin(t*angle)/s;}
    Quat q{};float n=0;
    for(int i=0;i<4;++i) {q[i]=a[i]*wa+b[i]*wb;n+=q[i]*q[i];}
    for(auto& x:q)x/=std::sqrt(n);
    return q;
}
Transform Mix(const Transform& a,const Transform& b,float t) {
    if(t<=0)return a;
    if(t>=1)return b;
    Transform v;
    for(int i=0;i<3;++i) {v.t[i]=a.t[i]+(b.t[i]-a.t[i])*t;v.s[i]=a.s[i]+(b.s[i]-a.s[i])*t;}
    v.q=Slerp(a.q,b.q,t);return v;
}
namespace {
bool Finite(const float* p,std::size_t n) {for(std::size_t i=0;i<n;++i) if(!std::isfinite(p[i]) || std::abs(p[i])>1e6f)return false;return true;}
bool Quaternion(const Quat& q) {float n=0;for(float v:q)n+=v*v;return Finite(q.data(),4)&&std::abs(n-1)<.002f;}
bool Read(const cgltf_accessor* a,std::size_t i,float* v,std::size_t n) {
    return a && cgltf_num_components(a->type)==n && cgltf_accessor_read_float(a,i,v,n) && Finite(v,n);
}
bool Parse(Library& out,cgltf_data* d) {
    const auto fail=[&](const char* s) {out.error=s;return false;};
    if(d->file_type!=cgltf_file_type_glb || d->buffers_count!=1 || d->buffers[0].uri ||
       d->buffers[0].size>d->bin_size || !d->bin || d->extensions_required_count || d->extensions_used_count)
        return fail("Expected self-contained GLB without required extensions");
    d->buffers[0].data=const_cast<void*>(d->bin);
    // Bounds before cgltf_validate/read: avoid overflow, sparse and compressed data.
    for(std::size_t i=0;i<d->buffer_views_count;++i) {
        const auto& v=d->buffer_views[i];
        if(v.buffer!=d->buffers || v.has_meshopt_compression || v.offset>d->buffers[0].size ||
           v.size>d->buffers[0].size-v.offset) return fail("Invalid buffer view");
    }
    for(std::size_t i=0;i<d->accessors_count;++i) {
        const auto& a=d->accessors[i];const auto bytes=cgltf_calc_size(a.type,a.component_type);
        if(a.is_sparse || !a.buffer_view || !bytes || !a.count || a.count>1000000 || a.stride<bytes ||
           a.offset>a.buffer_view->size || bytes>a.buffer_view->size-a.offset ||
           a.count-1>(a.buffer_view->size-a.offset-bytes)/a.stride)
            return fail("Invalid accessor bounds, sparse accessor or stride");
    }
    if(cgltf_validate(d)!=cgltf_result_success) return fail("glTF validation failed");
    if(!d->nodes_count || d->nodes_count>kMaxNodes || d->skins_count!=1 || d->meshes_count!=1 ||
       d->meshes[0].primitives_count!=1 || d->animations_count!=out.clips.size())
        return fail("Unsupported hand scene/clip counts");
    out.nodes.resize(d->nodes_count);
    int meshCount=0;
    for(std::size_t i=0;i<d->nodes_count;++i) {
        const auto& n=d->nodes[i];auto& dest=out.nodes[i];
        if(n.mesh) {++meshCount;if(n.skin!=d->skins)return fail("Mesh must use the hand skin");}
        if(n.has_mesh_gpu_instancing || n.weights_count) return fail("Instancing/morph weights unsupported");
        dest.parent=n.parent?static_cast<int>(n.parent-d->nodes):-1;
        std::copy_n(n.translation,3,dest.rest.t.begin());std::copy_n(n.rotation,4,dest.rest.q.begin());
        std::copy_n(n.scale,3,dest.rest.s.begin());dest.fixedMatrix=n.has_matrix;
        cgltf_node_transform_local(&n,dest.matrix.data());
        if(!Finite(dest.matrix.data(),16) || !Finite(dest.rest.t.data(),3) || !Quaternion(dest.rest.q) ||
           !Finite(dest.rest.s.data(),3)) return fail("Invalid node transform");
        for(float s:dest.rest.s) if(s<=1e-5f)return fail("Nonpositive node scale");
    }
    if(meshCount!=1)return fail("Expected one skinned mesh instance");
    // Parent-first evaluation independent of GLB node order, with cycle detection.
    std::array<bool,kMaxNodes> done{};
    while(out.order.size()<out.nodes.size()) {
        const auto before=out.order.size();
        for(std::size_t i=0;i<out.nodes.size();++i) if(!done[i] && (out.nodes[i].parent<0 || done[out.nodes[i].parent])) {
            out.order.push_back(static_cast<int>(i));done[i]=true;
        }
        if(out.order.size()==before)return fail("Cyclic hierarchy");
    }
    const auto& skin=d->skins[0];
    if(!skin.joints_count || skin.joints_count>kMaxJoints || !skin.inverse_bind_matrices ||
       skin.inverse_bind_matrices->count!=skin.joints_count) return fail("Invalid skin");
    for(std::size_t i=0;i<skin.joints_count;++i) {
        const int node=static_cast<int>(skin.joints[i]-d->nodes);
        if(std::find(out.joints.begin(),out.joints.end(),node)!=out.joints.end())return fail("Duplicate joint");
        out.joints.push_back(node);Mat m{};
        if(skin.joints[i]->name&&!std::strcmp(skin.joints[i]->name,"Index_Tip_Right")) {
            out.indexTipJoint=static_cast<int>(i);
            Mat world{};cgltf_node_transform_world(skin.joints[i],world.data());
            out.indexTipBind={world[12],world[13],world[14]};
        }
        if(!Read(skin.inverse_bind_matrices,i,m.data(),16) || std::abs(m[3])+std::abs(m[7])+std::abs(m[11])+std::abs(m[15]-1)>.001f)
            return fail("Invalid inverse bind matrix");
        out.inverseBind.push_back(m);
    }
    const auto& primitive=d->meshes[0].primitives[0];
    if(primitive.type!=cgltf_primitive_type_triangles || primitive.targets_count || primitive.has_draco_mesh_compression)
        return fail("Only uncompressed triangles without morphs supported");
    const cgltf_accessor *pos=nullptr,*normal=nullptr,*uv=nullptr,*js[2]{},*ws[2]{};
    for(std::size_t i=0;i<primitive.attributes_count;++i) {
        const auto& a=primitive.attributes[i];
        if(a.type==cgltf_attribute_type_position && a.index==0)pos=a.data;
        else if(a.type==cgltf_attribute_type_normal && a.index==0)normal=a.data;
        else if(a.type==cgltf_attribute_type_texcoord && a.index==0)uv=a.data;
        else if(a.type==cgltf_attribute_type_joints && a.index<2)js[a.index]=a.data;
        else if(a.type==cgltf_attribute_type_weights && a.index<2)ws[a.index]=a.data;
        else return fail("Unsupported vertex attribute");
    }
    if(!pos || !normal || !uv || !js[0] || !ws[0] || bool(js[1])!=bool(ws[1]) || pos->count>5000)
        return fail("Missing hand attributes");
    if(pos->component_type!=cgltf_component_type_r_32f || normal->component_type!=cgltf_component_type_r_32f)
        return fail("Position and normal attributes must be floats");
    for(auto a:{normal,uv,js[0],ws[0],js[1],ws[1]}) if(a && a->count!=pos->count)return fail("Attribute count mismatch");
    out.vertices.resize(pos->count);
    for(std::size_t i=0;i<pos->count;++i) {
        auto& v=out.vertices[i];
        if(!Read(pos,i,v.pos,3)||!Read(normal,i,v.normal,3)||!Read(uv,i,v.uv,2))return fail("Invalid vertex");
        float norm=0;for(float n:v.normal)norm+=n*n;
        if(std::abs(norm-1)>.01f)return fail("Non-unit normal");
        float total=0;
        for(int set=0;set<2;++set) if(js[set]) {
            cgltf_uint ids[4]{};
            if((js[set]->component_type!=cgltf_component_type_r_8u && js[set]->component_type!=cgltf_component_type_r_16u) ||
               js[set]->type!=cgltf_type_vec4 || js[set]->normalized ||
               !cgltf_accessor_read_uint(js[set],i,ids,4) || !Read(ws[set],i,v.weights+set*4,4))return fail("Invalid joint/weight accessor");
            for(int k=0;k<4;++k) {
                const int j=set*4+k;
                if(ids[k]>=out.joints.size() || v.weights[j]<0 || v.weights[j]>1)return fail("Joint index or weight out of range");
                v.joints[j]=static_cast<std::uint16_t>(ids[k]);total+=v.weights[j];
            }
        }
        if(std::abs(total-1)>.002f)return fail("Weights do not sum to one");
        for(auto& w:v.weights)w/=total;
    }
    const auto* idx=primitive.indices;
    if(!idx || idx->count>27000 || idx->count%3 || idx->type!=cgltf_type_scalar ||
       (idx->component_type!=cgltf_component_type_r_16u && idx->component_type!=cgltf_component_type_r_32u && idx->component_type!=cgltf_component_type_r_8u))
        return fail("Invalid triangle indices");
    for(std::size_t i=0;i<idx->count;++i) {
        const auto v=cgltf_accessor_read_index(idx,i);if(v>=out.vertices.size())return fail("Index outside mesh");
        out.indices.push_back(static_cast<std::uint16_t>(v));
    }
    // The hand renderer intentionally retains the existing skin/lighting model.
    // Require the exported single albedo material rather than ignoring new features.
    const auto* mat=primitive.material;
    if(!mat || !mat->has_pbr_metallic_roughness || mat->alpha_mode!=cgltf_alpha_mode_opaque ||
       mat->normal_texture.texture || mat->emissive_texture.texture || mat->occlusion_texture.texture ||
       mat->pbr_metallic_roughness.metallic_roughness_texture.texture)
        return fail("Unsupported hand material");
    const auto& tex=mat->pbr_metallic_roughness.base_color_texture;
    if(!tex.texture || tex.texcoord!=0 || tex.has_transform || !tex.texture->image ||
       !tex.texture->image->buffer_view || !tex.texture->image->mime_type ||
       std::strcmp(tex.texture->image->mime_type,"image/png"))return fail("Expected embedded albedo PNG");
    const auto* image=tex.texture->image->buffer_view;
    const auto* imageBytes=static_cast<const unsigned char*>(d->buffers[0].data)+image->offset;
    const unsigned char pngHeader[]{137,80,78,71,13,10,26,10};
    if(image->size<24 || std::memcmp(imageBytes,pngHeader,8) ||
       imageBytes[16]!=0 || imageBytes[17]!=0 || imageBytes[18]!=4 || imageBytes[19]!=0 ||
       imageBytes[20]!=0 || imageBytes[21]!=0 || imageBytes[22]!=4 || imageBytes[23]!=0)
        return fail("Hand albedo must be a 1024x1024 PNG");
    for(float v:mat->pbr_metallic_roughness.base_color_factor)if(v!=1.f)return fail("Unsupported material color factor");
    out.albedoPng.assign(imageBytes,imageBytes+image->size);
    std::array<bool,handanimation::kNames.size()> found{};
    for(std::size_t i=0;i<d->animations_count;++i) {
        const auto& animation=d->animations[i];int number=-1;
        for(std::size_t j=0;j<found.size();++j)if(animation.name && !std::strcmp(animation.name,handanimation::kNames[j]))number=static_cast<int>(j);
        if(number<0 || found[number])return fail("Unknown/duplicate clip");found[number]=true;
        auto& clip=out.clips[number];
        if(!animation.channels_count || animation.channels_count>kMaxNodes*3)return fail("Invalid channel count");
        std::array<unsigned,kMaxNodes> channels{};
        for(std::size_t j=0;j<animation.channels_count;++j) {
            const auto& channel=animation.channels[j];const auto* s=channel.sampler;
            if(!channel.target_node || !s || !s->input || !s->output)return fail("Missing channel target/sampler");
            Track track;track.node=static_cast<int>(channel.target_node-d->nodes);
            if(out.nodes[track.node].fixedMatrix)return fail("Animated matrix node requires TRS");
            unsigned mask=0;std::size_t width=3;
            if(channel.target_path==cgltf_animation_path_type_translation){track.path=Path::Translation;mask=1;}
            else if(channel.target_path==cgltf_animation_path_type_rotation){track.path=Path::Rotation;mask=2;width=4;}
            else if(channel.target_path==cgltf_animation_path_type_scale){track.path=Path::Scale;mask=4;}
            else return fail("Morph/unknown animation path unsupported");
            if(channels[track.node]&mask)return fail("Duplicate animation channel");channels[track.node]|=mask;
            if(s->interpolation!=cgltf_interpolation_type_linear && s->interpolation!=cgltf_interpolation_type_step)
                return fail("Unsupported interpolation: bake CUBICSPLINE to bone LINEAR keys");
            track.step=s->interpolation==cgltf_interpolation_type_step;
            if(s->input->count<2 || s->input->count>4096 || s->output->count!=s->input->count ||
               s->input->component_type!=cgltf_component_type_r_32f || s->output->component_type!=cgltf_component_type_r_32f)
                return fail("Invalid animation samples");
            for(std::size_t k=0;k<s->input->count;++k) {
                float time;Quat value{};
                if(!Read(s->input,k,&time,1)||time<0 || (k && time<=track.times.back()) || !Read(s->output,k,value.data(),width))
                    return fail("Invalid animation time/value");
                if(track.path==Path::Rotation && !Quaternion(value))return fail("Non-unit animation quaternion");
                if(track.path==Path::Scale)for(int axis=0;axis<3;++axis)if(value[axis]<=1e-5f)return fail("Nonpositive animated scale");
                track.times.push_back(time);track.values.push_back(value);
            }
            if(track.times.front()!=0)return fail("Hand clips must start at time zero");
            clip.duration=std::max(clip.duration,track.times.back());clip.tracks.push_back(std::move(track));
        }
        if(clip.duration<=0)return fail("Empty animation range");
    }
    Pose rest{};for(std::size_t i=0;i<out.nodes.size();++i)rest[i]=out.nodes[i].rest;
    Palette bind{};out.Matrices(rest,bind);
    for(std::size_t i=0;i<out.joints.size();++i)for(int k=0;k<16;++k)
        if(std::abs(bind[i][k]-Identity()[k])>2e-4f)
            return fail("Hand profile requires identity mesh bind pose");
    out.loaded=true;return true;
}
} // namespace
bool Library::Load(const std::string& path) {
    std::unique_ptr<FILE,decltype(&std::fclose)> file(std::fopen(path.c_str(),"rb"),std::fclose);
    if(!file){error="Cannot open GLB";return false;}
    if(std::fseek(file.get(),0,SEEK_END)){error="Cannot seek GLB";return false;}
    const long size=std::ftell(file.get());
    if(size<20 || size>16*1024*1024){error="Invalid GLB file size";return false;}
    std::rewind(file.get());std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    if(std::fread(bytes.data(),1,bytes.size(),file.get())!=bytes.size()){error="Short GLB read";return false;}
    std::uint32_t declared=0;std::memcpy(&declared,bytes.data()+8,4);
    if(declared!=bytes.size()){error="GLB length mismatch";return false;}
    struct Budget { std::size_t remaining=32*1024*1024; } budget;
    cgltf_options options{};
    options.memory.user_data=&budget;
    options.memory.alloc_func=[](void* user,cgltf_size size)->void* {
        auto& b=*static_cast<Budget*>(user);
        if(size>b.remaining)return nullptr;
        b.remaining-=size;return std::malloc(size);
    };
    options.memory.free_func=[](void*,void* p) {std::free(p);};
    cgltf_data* raw=nullptr;
    if(cgltf_parse(&options,bytes.data(),bytes.size(),&raw)!=cgltf_result_success){error="GLB parse failed";return false;}
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
    Library fresh;
    if(!Parse(fresh,raw)){error=fresh.error;return false;}
    *this=std::move(fresh);return true;
}
void Library::Sample(handanimation::Gesture gesture,float progress,Pose& out) const {
    for(std::size_t i=0;i<nodes.size();++i)out[i]=nodes[i].rest;
    const auto& clip=clips[static_cast<int>(gesture)];const float time=handanimation::Unit(progress)*clip.duration;
    for(const auto& track:clip.tracks) {
        auto next=std::upper_bound(track.times.begin(),track.times.end(),time);
        const std::size_t b=next==track.times.end()?track.times.size()-1:static_cast<std::size_t>(next-track.times.begin());
        const std::size_t a=b?b-1:0;
        const float t=(a==b || track.step)?0.f:std::clamp((time-track.times[a])/(track.times[b]-track.times[a]),0.f,1.f);
        // At the last STEP key select that key, not the preceding interval.
        const auto& first=track.values[time>=track.times.back()?b:a];const auto& last=track.values[b];
        auto& transform=out[track.node];
        if(track.path==Path::Rotation)transform.q=Slerp(first,last,t);
        else {auto& v=track.path==Path::Translation?transform.t:transform.s;for(int k=0;k<3;++k)v[k]=first[k]+(last[k]-first[k])*t;}
    }
}
void Library::Blend(const Pose& a,const Pose& b,float weight,Pose& out) const {
    const float t=handanimation::Unit(weight);for(std::size_t i=0;i<nodes.size();++i)out[i]=Mix(a[i],b[i],t);
}
void Library::Matrices(const Pose& pose,Palette& out) const {
    std::array<Mat,kMaxNodes> world{};
    for(int i:order) {const auto local=nodes[i].fixedMatrix?nodes[i].matrix:Matrix(pose[i]);
        world[i]=nodes[i].parent<0?local:Multiply(world[nodes[i].parent],local);}
    for(std::size_t i=0;i<joints.size();++i)out[i]=Multiply(world[joints[i]],inverseBind[i]);
}
void Library::Evaluate(const handanimation::Selection& selection,float pinchProgress,float pinchWeight,Palette& out) const {
    Pose a{},b{},pose{};Sample(selection.first,selection.progress,a);Sample(selection.second,selection.progress,b);
    Blend(a,b,selection.blend,pose);
    if(pinchWeight>0) {Sample(handanimation::Gesture::Pinch,pinchProgress,a);Blend(pose,a,pinchWeight,pose);}
    if(selection.openPalmBlend>0) {Sample(handanimation::Gesture::OpenPalm,1.f,a);Blend(pose,a,selection.openPalmBlend,pose);}
    if(selection.pointBlend>0) {Sample(handanimation::Gesture::Point,1.f,a);Blend(pose,a,selection.pointBlend,pose);}
    Matrices(pose,out);
}
bool Library::IndexTip(const Palette& palette,Vec& out) const {
    if(indexTipJoint<0||indexTipJoint>=static_cast<int>(joints.size()))return false;
    const auto& m=palette[indexTipJoint];const auto& p=indexTipBind;
    for(int i=0;i<3;++i)out[i]=m[i]*p[0]+m[4+i]*p[1]+m[8+i]*p[2]+m[12+i];
    return true;
}
std::size_t Library::ResidentBytes() const {
    std::size_t bytes=sizeof(*this)+vertices.capacity()*sizeof(Vertex)+indices.capacity()*sizeof(std::uint16_t)+
        nodes.capacity()*sizeof(Node)+order.capacity()*sizeof(int)+joints.capacity()*sizeof(int)+inverseBind.capacity()*sizeof(Mat)+albedoPng.capacity();
    for(const auto& clip:clips) {bytes+=clip.tracks.capacity()*sizeof(Track);
        for(const auto& t:clip.tracks)bytes+=t.times.capacity()*sizeof(float)+t.values.capacity()*sizeof(Quat);}
    return bytes;
}
} // namespace savr::handskeleton
