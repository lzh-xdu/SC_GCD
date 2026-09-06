#include <systemc>
#include <iostream>

// Both methods run at the same rising edge. sc_signal writes become visible
// during the subsequent update phase, so stage2 reads stage1's previous value.
SC_MODULE(TwoRegisters) {
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<int> data_in{"data_in"};
    sc_core::sc_out<int> data_out{"data_out"};
    sc_core::sc_signal<int> stage1{"stage1"};

    void capture() { stage1.write(data_in.read()); }
    void forward() { data_out.write(stage1.read()); }

    SC_CTOR(TwoRegisters) {
        SC_METHOD(capture);
        sensitive << clk.pos();
        dont_initialize();
        SC_METHOD(forward);
        sensitive << clk.pos();
        dont_initialize();
    }
};

SC_MODULE(Testbench) {
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_out<int> stimulus_out{"stimulus_out"};
    sc_core::sc_in<int> result_in{"result_in"};
    int failures = 0;

    void check(int expected, const char* phase) {
        std::cout << sc_core::sc_time_stamp() << " | " << phase
                  << " | output=" << result_in.read()
                  << " expected=" << expected << '\n';
        if (result_in.read() != expected) {
            ++failures;
            std::cerr << "FAIL: " << phase << '\n';
        }
    }

    void run() {
        stimulus_out.write(10);
        wait(clk.posedge_event()); // 5 ns: stage1 captures 10, output stays 0.
        check(0, "edge 1 before update");
        wait(sc_core::SC_ZERO_TIME);
        check(0, "edge 1 after update");

        wait(clk.negedge_event()); // Drive away from the sampling edge.
        stimulus_out.write(20);
        wait(clk.posedge_event()); // 15 ns: stage1 captures 20, output gets 10.
        check(0, "edge 2 before update");
        wait(sc_core::SC_ZERO_TIME);
        check(10, "edge 2 after update");

        wait(clk.negedge_event());
        stimulus_out.write(30);
        wait(clk.posedge_event()); // 25 ns: output gets 20.
        check(10, "edge 3 before update");
        wait(sc_core::SC_ZERO_TIME);
        check(20, "edge 3 after update");
        if (sc_core::sc_time_stamp() != sc_core::sc_time(25, sc_core::SC_NS)) {
            ++failures;
        }
        sc_core::sc_stop();
    }

    SC_CTOR(Testbench) { SC_THREAD(run); }
};

int sc_main(int, char**) {
    sc_core::sc_clock clk("clk", sc_core::sc_time(10, sc_core::SC_NS),
                          0.5, sc_core::sc_time(5, sc_core::SC_NS), true);
    sc_core::sc_signal<int> tb_to_registers_data("tb_to_registers_data");
    sc_core::sc_signal<int> registers_to_tb_data("registers_to_tb_data");
    TwoRegisters model("model");
    model.clk(clk);
    model.data_in(tb_to_registers_data);
    model.data_out(registers_to_tb_data);
    Testbench tb("tb");
    tb.clk(clk);
    tb.stimulus_out(tb_to_registers_data);
    tb.result_in(registers_to_tb_data);

    auto* trace = sc_core::sc_create_vcd_trace_file("day01_basics");
    trace->set_time_unit(1, sc_core::SC_NS);
    sc_core::sc_trace(trace, clk, "clk");
    sc_core::sc_trace(trace, tb_to_registers_data, "tb_to_registers_data");
    sc_core::sc_trace(trace, model.stage1, "stage1");
    sc_core::sc_trace(trace, registers_to_tb_data, "registers_to_tb_data");
    sc_core::sc_start(sc_core::sc_time(100, sc_core::SC_NS));
    sc_core::sc_close_vcd_trace_file(trace);
    if (!sc_core::sc_end_of_simulation_invoked()) {
        std::cerr << "FAIL: test did not finish before timeout\n";
        return 1;
    }
    std::cout << (tb.failures == 0 ? "PASS" : "FAIL") << '\n';
    return tb.failures == 0 ? 0 : 1;
}
