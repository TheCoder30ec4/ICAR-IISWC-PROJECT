import socket
import streamlit as st
from streamlit.components.v1 import html
import os 

# Custom CSS styling
def inject_css():
    st.markdown("""
    <style>
        .main {
            background-color: #f5f5f5;
        }
        .logo-container {
            text-align: center;
            margin-bottom: 20px;
        }
        .header-text {
            color: #2c3e50;
            text-align: center;
            font-size: 2.2em;
            font-weight: bold;
            margin-bottom: 25px;
        }
        .stTextInput input {
            border-radius: 8px;
            padding: 12px;
            font-size: 16px;
        }
        .stButton button {
            background-color: #4CAF50;
            color: white;
            border-radius: 8px;
            padding: 12px 24px;
            width: 100%;
            font-size: 16px;
            font-weight: bold;
            transition: all 0.3s;
        }
        .stButton button:hover {
            background-color: #45a049;
            transform: scale(1.05);
        }
        .status-box {
            padding: 18px;
            border-radius: 10px;
            margin: 20px 0;
            text-align: center;
            font-size: 16px;
            font-weight: bold;
        }
        .connected {
            background-color: #d4edda;
            color: #155724;
        }
        .disconnected {
            background-color: #f8d7da;
            color: #721c24;
        }
    </style>
    """, unsafe_allow_html=True)

# Function to validate IP address format
def is_valid_ip(ip):
    try:
        socket.inet_aton(ip)
        return True
    except socket.error:
        return False

# Function to send ESP32 IP to ESP8266
def send_ip_to_esp8266():
    ESP8266_IP = st.session_state.esp8266_ip
    ESP32_IP = st.session_state.esp32_ip
    TCP_PORT = 12345

    if not (is_valid_ip(ESP8266_IP) and is_valid_ip(ESP32_IP)):
        st.session_state.connection_status = "❌ Invalid IP format"
        st.session_state.connected = False
        return

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5)
            s.connect((ESP8266_IP, TCP_PORT))
            s.sendall(f"{ESP32_IP}\n".encode())
            st.session_state.connected = True
            st.session_state.connection_status = f"✅ Successfully connected to {ESP8266_IP}"
    except socket.timeout:
        st.session_state.connection_status = "⚠️ Connection timeout"
        st.session_state.connected = False
    except Exception as e:
        st.session_state.connection_status = f"❌ Connection error: {str(e)}"
        st.session_state.connected = False

# Streamlit UI Configuration

image_dir = os.path.dirname(os.path.abspath(__file__))
image_path = os.path.join(image_dir,"./logo.png")
st.set_page_config(
    page_title="IoT Irrigation Control",
    page_icon="🌾",
    layout="centered"
)

# Initialize session state
if 'connected' not in st.session_state:
    st.session_state.connected = False
if 'connection_status' not in st.session_state:
    st.session_state.connection_status = "Not connected"

# Inject custom CSS
inject_css()

# Main Interface
# st.markdown('<div class="logo-container">', unsafe_allow_html=True)
# st.image("./logo.png", width=800)  # Adjust width for a balanced look
# st.markdown('</div>', unsafe_allow_html=True)

st.markdown("<h1 class='header-text'>Real-Time Water Level Monitoring & Irrigation Control</h1>", 
            unsafe_allow_html=True)

# Connection Form
with st.form("connection_form", border=True):
    st.subheader("🌐 Device Configuration")
    
    col1, col2 = st.columns(2)
    with col1:
        st.text_input("ESP8266 IP Address", 
                     key="esp8266_ip",
                     placeholder="e.g., 192.168.1.100",
                     help="Enter the IP address of your ESP8266 module")
    with col2:
        st.text_input("ESP32 IP Address", 
                     key="esp32_ip",
                     placeholder="e.g., 192.168.1.101",
                     help="Enter the IP address of your ESP32 module")
    
    submitted = st.form_submit_button("🚀 Connect Devices", 
                                     help="Click to establish connection between ESP32 and ESP8266")
    if submitted:
        send_ip_to_esp8266()
    if st.session_state.connected:
        st.success("✅ Connected successfully!")

# Connection Status Display
status_class = "connected" if st.session_state.connected else "disconnected"
html(f"""
<div class="status-box {status_class}">
    <h3>📡 Connection Status</h3>
    <p>{st.session_state.connection_status}</p>
</div>
""")

# System Information
with st.expander("ℹ️ System Information", expanded=False):
    st.markdown("""
    **🔧 System Components:**
    - 🟢 **ESP32:** Water level sensing & motor control
    - 🔵 **ESP8266:** Gateway for data transmission
    - ☁️ **Cloud Dashboard:** Real-time monitoring & analytics
    
    **⚡ Connection Requirements:**
    1️⃣ Both devices must be on the same WiFi network  
    2️⃣ Ensure port forwarding is enabled for remote access  
    3️⃣ Maintain stable power for continuous operation  
    """)

# Footer
st.markdown("---")
st.markdown("<div style='text-align: center; color: #666;'>🌾 Sustainable Agriculture IoT System © 2025</div>", 
            unsafe_allow_html=True)
